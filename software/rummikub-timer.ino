#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== Display setup =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== Pins =====
const int BTN_START = 2;
const int BTN_PLUS  = 3;
const int BTN_MINUS = 4;
const int BUZZER    = 9;
const int BATT_PIN  = A1;
const int LED_PIN = 10;   // transistor controlling arcade LED
bool ledState = false;
unsigned long lastBlink = 0;

// ===== State machine =====
enum State { IDLE, RUNNING, ALARM };
State state = IDLE;

// ===== Timer =====
unsigned long prevMillis = 0;
unsigned long lastSecond = 0;
int setSeconds = 60;      // default: 1:00
int remaining = 60;
const int STEP = 10;      // adjust in 10s steps

// ===== Helper: battery =====
float readBatteryVoltage() {
  int raw = analogRead(BATT_PIN);
  float vAdc = (raw / 1023.0) * 5.0;    // Nano = 5V ADC ref
  float vBat = vAdc * 3.2;              // Divider: (220k+100k)/100k = 3.2
  return vBat;
}

int batteryPercent() {
  float v = readBatteryVoltage();
  if (v > 4.2) v = 4.2;
  if (v < 3.3) v = 3.3;
  return (int)((v - 3.3) * 100.0 / (4.2 - 3.3)); // map 3.3–4.2V → 0–100%
}

// ===== Helper: draw battery icon =====
void drawBatteryIcon(int x, int y, int percent) {
  display.drawRect(x, y, 18, 8, SSD1306_WHITE);   // outline
  display.fillRect(x+18, y+2, 2, 4, SSD1306_WHITE); // nub
  int bars = map(percent, 0, 100, 0, 4);
  for (int i=0; i<bars; i++) {
    display.fillRect(x+2 + i*4, y+2, 3, 4, SSD1306_WHITE);
  }
}

// ===== Format MM:SS =====
void drawTimeLarge(int secs) {
  int m = secs / 60;
  int s = secs % 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", m, s);
  display.setTextSize(4);
  display.setCursor(6, 24);
  display.print(buf);
}

void setup() {
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_PLUS, INPUT_PULLUP);
  pinMode(BTN_MINUS, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED on in idle

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); // hang if OLED not found
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long now = millis();

  // ===== Read buttons (active LOW) =====
  bool startPressed = !digitalRead(BTN_START);
  bool plusPressed  = !digitalRead(BTN_PLUS);
  bool minusPressed = !digitalRead(BTN_MINUS);

  // ===== State machine =====
  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, HIGH); // always on
      if (plusPressed) {
        setSeconds += STEP;
        delay(200); // crude debounce
      }
      if (minusPressed && setSeconds > STEP) {
        setSeconds -= STEP;
        delay(200);
      }
      if (startPressed) {
        remaining = setSeconds;
        lastSecond = now;
        state = RUNNING;
        delay(200);
      }
      break;

    case RUNNING:
      if (remaining <= 10) {
          if (now - lastBlink >= 500) {  // blink every 500 ms
            lastBlink = now;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
          }
      } else {
          digitalWrite(LED_PIN, HIGH); // solid on until last 10s
      }
      if (startPressed) { // restart
        remaining = setSeconds;
        lastSecond = now;
        delay(200);
      }
      if (now - lastSecond >= 1000) { // tick
        lastSecond += 1000;
        if (remaining > 0) {
          remaining--;
        } else {
          state = ALARM;
        }
      }
      break;

    case ALARM:
      digitalWrite(LED_PIN, LOW); // off
      digitalWrite(BUZZER, HIGH);
      delay(1000);
      digitalWrite(BUZZER, LOW);
      state = IDLE;
      break;
  }

  // ===== Display update =====
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  int batt = batteryPercent();
  drawBatteryIcon(104, 0, batt);

  if (state == IDLE) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Set:");
    drawTimeLarge(setSeconds);
  }
  else if (state == RUNNING) {
    drawTimeLarge(remaining);
  }
  else if (state == ALARM) {
    display.setTextSize(2);
    display.setCursor(10, 24);
    display.print("TIME UP!");
  }

  display.display();
}
