#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <time.h>
#include <math.h>
#include "tavolab_logo_240x240.h"  // Array da imagem de logo

// --- Protótipos ---
void drawScreen(int screen);
void drawClock();
void drawClimate();
void startBreathAnimation();
void drawBreathAnimation();
void drawSmoothPetal(int cx, int cy, float radius, float angleOffset, uint16_t color);
bool setupWiFi();
void resetWiFiSettings();
void setupTime();

// --- Pinos ---
const int btnLeft   = 26;
const int btnRight  = 27;
const int btnCenter = 14;

// --- Estado das telas e botões ---
int currentScreen = 0;
const int numScreens = 3;
unsigned long lastPress = 0;
const int debounceTime = 300;
unsigned long btnCenterPressTime = 0;
bool btnCenterPressed = false;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 1000;  // 1 segundo

// --- Display ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// --- Sensor AHT10 ---
Adafruit_AHTX0 aht;
float temperatura = 0.0;
float umidade = 0.0;

// --- Respiração ---
int frame = 0;
int cycleCount = 0;
bool showMessages = true;
bool wasGrowing = false;
bool breathStarted = false;

// --- Relógio via NTP ---
void setupTime() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

// --- Telas principais ---
void drawScreen(int screen) {
  tft.fillScreen(TFT_BLACK);
  switch (screen) {
    case 0: drawClock(); break;
    case 1: drawClimate(); break;
    case 2: startBreathAnimation(); break;
  }
}

void drawClock() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(4);
    tft.drawString(timeStr, 120, 120);
  } else {
    tft.setTextSize(2);
    tft.drawString("Sem hora :(", 120, 120);
  }
}

void drawClimate() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  temperatura = temp.temperature;
  umidade = humidity.relative_humidity;

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Clima", 120, 30);

  char tempStr[20];
  sprintf(tempStr, "Temp: %.1f C", temperatura);
  tft.drawString(tempStr, 120, 100);

  char humStr[20];
  sprintf(humStr, "Umidade: %.1f %%", umidade);
  tft.drawString(humStr, 120, 150);
}

void startBreathAnimation() {
  if (!breathStarted) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Vamos inicar?", 120, 100);
    delay(1000);
    tft.drawString("1...", 120, 140); delay(1000);
    tft.drawString("2...", 120, 140); delay(1000);
    tft.drawString("3...", 120, 140); delay(1000);
    tft.fillScreen(TFT_BLACK);
    spr.createSprite(200, 200);
    spr.setSwapBytes(true);
    breathStarted = true;
    frame = 0;
    cycleCount = 0;
    showMessages = true;
  }
}

void drawBreathAnimation() {
  spr.fillSprite(TFT_BLACK);

  int centerX = 100;
  int centerY = 100;
 float breath = sin(frame * 0.05) * 0.5 + 0.5;
  bool isGrowing = (sin(frame * 0.15) > 0);

  if (isGrowing && !wasGrowing) {
    cycleCount++;
    if (cycleCount > 4) showMessages = false;
  }
  wasGrowing = isGrowing;

  for (int i = 0; i < 6; i++) {
    float angleOffset = i * (2 * PI / 6);
    uint16_t color = tft.color565(180 + i * 10, 80 + i * 5, 255 - i * 20);
    drawSmoothPetal(centerX, centerY, 40 + breath * 40, angleOffset, color);
  }

  if (showMessages) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextSize(2);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    if (isGrowing) spr.drawString("Puxe o ar...", 100, 180);
    else spr.drawString("Solte o ar...", 100, 180);
  }

  spr.pushSprite(20, 20);
  frame++;
}

void drawSmoothPetal(int cx, int cy, float radius, float angleOffset, uint16_t color) {
  const int points = 60;
  int lastX = -1, lastY = -1;

  for (int i = 0; i <= points; i++) {
    float angle = (2 * PI / points) * i;
    float wave = sin(3 * angle);
    float r = radius + wave * 15;

    float finalAngle = angle + angleOffset;
    int x = cx + cos(finalAngle) * r;
    int y = cy + sin(finalAngle) * r;

    if (lastX >= 0 && lastY >= 0) {
      spr.drawLine(lastX, lastY, x, y, color);
    }
    lastX = x;
    lastY = y;
  }
}

// --- Wi-Fi ---
void resetWiFiSettings() {
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

bool setupWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("AURA")) {
    Serial.println("Falha na conexão.");
    return false;
  }
  Serial.println("Conectado ao Wi-Fi!");
  return true;
}

// --- Setup principal ---
void setup() {
  Serial.begin(115200);

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnCenter, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);

  // Splash logo
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, 240, 240, tavolabLogo);
  delay(3000);

  // Mensagem de boas-vindas
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Oie, tudo bem?", 120, 80);
  tft.drawString("Vamos nos conectar", 120, 120);
  tft.drawString("ao Wi-Fi?", 120, 160);
  delay(3000);

  Wire.begin(21, 22);
  delay(100);

  if (!aht.begin()) {
    tft.drawString("AHT10 nao encontrado", 120, 160);
    Serial.println("Erro ao iniciar AHT10");
    while (1) delay(10);
  }

  if (!setupWiFi()) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Conecte-se ao Wi-Fi", 120, 120);
    while (1) delay(10);
  }

  setupTime();
  drawScreen(currentScreen);
}

// --- Loop principal ---
void loop() {
  // Navegação por botões
  if (digitalRead(btnLeft) == LOW && millis() - lastPress > debounceTime) {
    currentScreen = (currentScreen - 1 + numScreens) % numScreens;
    drawScreen(currentScreen);
    breathStarted = false;
    lastPress = millis();
  }

  if (digitalRead(btnRight) == LOW && millis() - lastPress > debounceTime) {
    currentScreen = (currentScreen + 1) % numScreens;
    drawScreen(currentScreen);
    breathStarted = false;
    lastPress = millis();
  }

  // Reset Wi-Fi com botão central
  if (digitalRead(btnCenter) == LOW) {
    if (!btnCenterPressed) {
      btnCenterPressed = true;
      btnCenterPressTime = millis();
    } else if (millis() - btnCenterPressTime > 5000) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.drawString("Redefinindo Wi-Fi...", 120, 120);
      resetWiFiSettings();
    }
  } else {
    btnCenterPressed = false;
  }

  // Atualizações por tela
  if (currentScreen == 0 || currentScreen == 1) {
    if (millis() - lastUpdate > updateInterval) {
      if (currentScreen == 0) drawClock();
      else if (currentScreen == 1) drawClimate();
      lastUpdate = millis();
    }
  } else if (currentScreen == 2) {
    drawBreathAnimation();  // animação contínua e suave
  }

  delay(50);  // loop suave
}