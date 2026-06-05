#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// =========================
// PIN CONFIG
// =========================
#define FLAME_PIN   34
#define TRIG_PIN    5
#define ECHO_PIN    18
#define RELAY_PIN   26
#define SERVO_PIN   13
#define BUZZER_PIN  25
#define LED_RED     14
#define LED_GREEN   27
#define BTN_PIN     33

// =========================
// LCD
// =========================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// SERVO
// =========================
Servo myservo;

// =========================
// MUTEX
// =========================
SemaphoreHandle_t xMutex;

// =========================
// GLOBAL VARIABLE
// =========================
int nilaiApi = 0;
long jarak = 0;

bool statusBahaya = false;
bool tombolDitekan = false;

// Kalibrasi setelah melihat Serial Monitor
const int BATAS_API = 2500;

// =========================
// TASK PROTOTYPE
// =========================
void TaskSensor(void *pvParameters);
void TaskAlarm(void *pvParameters);
void TaskDisplay(void *pvParameters);

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  analogReadResolution(12);

  pinMode(FLAME_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  pinMode(BTN_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, HIGH);

  // Servo
  myservo.setPeriodHertz(50);
  myservo.attach(SERVO_PIN, 500, 2400);
  myservo.write(90);

  // LCD I2C
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FIRE SYSTEM");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);

  lcd.clear();

  // Mutex
  xMutex = xSemaphoreCreateMutex();

  if (xMutex == NULL) {
    Serial.println("Mutex Error");
    while (1);
  }

  // Task
  xTaskCreatePinnedToCore(
      TaskSensor,
      "TaskSensor",
      4096,
      NULL,
      3,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      TaskAlarm,
      "TaskAlarm",
      4096,
      NULL,
      2,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      TaskDisplay,
      "TaskDisplay",
      4096,
      NULL,
      1,
      NULL,
      0);
}

// =========================
// LOOP
// =========================
void loop() {
  vTaskDelay(portMAX_DELAY);
}

// =========================
// TASK SENSOR
// =========================
void TaskSensor(void *pvParameters) {

  int counterApi = 0;
  int counterAman = 0;

  for (;;) {

    // =====================
    // FLAME SENSOR
    // =====================
    int api = analogRead(FLAME_PIN);

    bool bahayaBaru = statusBahaya;

    if (api < BATAS_API) {
      counterApi++;
      counterAman = 0;

      if (counterApi >= 3) {
        bahayaBaru = true;
      }

    } else {

      counterAman++;
      counterApi = 0;

      if (counterAman >= 3) {
        bahayaBaru = false;
      }
    }

    // =====================
    // ULTRASONIC
    // =====================
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    long duration =
      pulseIn(ECHO_PIN, HIGH, 30000);

    long distance;

    if (duration == 0) {
      distance = 999;
    }
    else {
      distance =
        duration * 0.0343 / 2.0;
    }

    // =====================
    // RESET BUTTON
    // =====================
    bool btn =
      (digitalRead(BTN_PIN) == LOW);

    if (btn) {

      bahayaBaru = false;

      counterApi = 0;
      counterAman = 0;
    }

    // =====================
    // UPDATE GLOBAL
    // =====================
    if (xSemaphoreTake(
            xMutex,
            pdMS_TO_TICKS(10))
        == pdTRUE) {

      nilaiApi = api;
      jarak = distance;

      statusBahaya = bahayaBaru;
      tombolDitekan = btn;

      xSemaphoreGive(xMutex);
    }

    // =====================
    // DEBUG
    // =====================
    Serial.print("Flame: ");
    Serial.print(api);

    Serial.print(" | Jarak: ");
    Serial.print(distance);

    Serial.print(" cm | ");

    if (bahayaBaru)
      Serial.println("BAHAYA");
    else
      Serial.println("AMAN");

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// =========================
// TASK ALARM
// =========================
void TaskAlarm(void *pvParameters) {

  for (;;) {

    bool bahaya;

    if (xSemaphoreTake(
            xMutex,
            pdMS_TO_TICKS(10))
        == pdTRUE) {

      bahaya = statusBahaya;

      xSemaphoreGive(xMutex);
    }
    else {
      vTaskDelay(50);
      continue;
    }

    if (bahaya) {

      digitalWrite(
        LED_GREEN,
        LOW);

      digitalWrite(
        LED_RED,
        HIGH);

      digitalWrite(
        RELAY_PIN,
        HIGH);

      digitalWrite(
        BUZZER_PIN,
        HIGH);

      for (int pos = 60;
           pos <= 120;
           pos += 5) {

        if (!statusBahaya)
          break;

        myservo.write(pos);

        vTaskDelay(
          pdMS_TO_TICKS(30));
      }

      for (int pos = 120;
           pos >= 60;
           pos -= 5) {

        if (!statusBahaya)
          break;

        myservo.write(pos);

        vTaskDelay(
          pdMS_TO_TICKS(30));
      }

    } else {

      digitalWrite(
        LED_GREEN,
        HIGH);

      digitalWrite(
        LED_RED,
        LOW);

      digitalWrite(
        RELAY_PIN,
        LOW);

      digitalWrite(
        BUZZER_PIN,
        LOW);

      myservo.write(90);

      vTaskDelay(
        pdMS_TO_TICKS(100));
    }
  }
}

// =========================
// TASK LCD
// =========================
void TaskDisplay(void *pvParameters) {

  for (;;) {

    bool bahaya;
    bool tombol;

    int api;
    long distance;

    if (xSemaphoreTake(
            xMutex,
            pdMS_TO_TICKS(10))
        == pdTRUE) {

      bahaya = statusBahaya;
      tombol = tombolDitekan;

      api = nilaiApi;
      distance = jarak;

      xSemaphoreGive(xMutex);
    }
    else {
      vTaskDelay(300);
      continue;
    }

    lcd.clear();

    if (tombol) {

      lcd.setCursor(0, 0);
      lcd.print("SYSTEM RESET");

    }
    else if (bahaya) {

      lcd.setCursor(0, 0);
      lcd.print("!!! ADA API !!!");

      lcd.setCursor(0, 1);
      lcd.print("Jarak:");
      lcd.print(distance);
      lcd.print("cm");
    }
    else {

      lcd.setCursor(0, 0);
      lcd.print("STATUS: AMAN");

      lcd.setCursor(0, 1);
      lcd.print("Api:");
      lcd.print(api);
    }

    vTaskDelay(
      pdMS_TO_TICKS(500));
  }
}
