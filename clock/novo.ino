#define DEBUG_PRINT
#define DEBUG_VARS
//#define DEBUG_SCREEN

#ifdef DEBUG_PRINT
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

// #include <LiquidCrystal_I2C.h>
// LiquidCrystal_I2C lcd(0x27,16,4);

#include <LiquidCrystal.h>               // includes the LiquidCrystal Library
#define LCD_COLONS 16
#define LCD_ROWS 4
LiquidCrystal lcd(6, A1, 9, 10, 11, 12);  // Creates an LCD object. Parameters: (rs, enable, d4, d5, d6, d7)

#include <LibPrintf.h>

#include <Wire.h>

#include <PCF85063A-SOLDERED.h>
PCF85063A rtc;

#include <BMP180I2C.h>
#define TEMP_SENSOR_ADDRESS 0x77
BMP180I2C bmp180(TEMP_SENSOR_ADDRESS);

// #include <LiquidCrystal.h>

#define USE_TIMER_1 true
#define USE_TIMER_2 false
#include <TimerInterrupt.h>
#include <TimerInterrupt.hpp>
#include <ISR_Timer.h>
#include <ISR_Timer.hpp>

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, 8, NEO_GRB + NEO_KHZ800);

char tempToRGB[16][3] = {
    {0, 0, 255},     // 15°C - Blue
    {0, 51, 255},    // 16°C
    {0, 102, 255},   // 17°C
    {0, 153, 255},   // 18°C
    {0, 204, 255},   // 19°C
    {0, 255, 255},   // 20°C
    {0, 255, 204},   // 21°C
    {0, 255, 153},   // 22°C
    {0, 255, 75},     // 23°C - Green
    {0, 255, 0},   // 24°C - Perfect Temperature (Green)
    {75, 255, 0},     // 25°C - Green
    {128, 255, 0},   // 26°C
    {204, 255, 0},   // 27°C - Yellow
    {255, 128, 0},   // 28°C
    {255, 88, 0},     // 29°C
    {255, 44, 0}      // 30°C - Red
};

typedef struct Time {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  int year;
  uint8_t weekdayIndex;
  char weekday[4];
} Time;

typedef struct Sensor {
  int temperature;
  float pressure;
  bool tempStarted = false;
  unsigned long tempLastTime = 0;
  bool pressStarted = false;
  unsigned long pressLastTime = 0;
} Sensor;

typedef struct Button {
  int pin;
  int lastStatus;
  int longPress;
  long since;
} Button;

typedef struct Display {
  char line0[17];
  char line1[17];
  char line2[17];
  char line3[17];
} Display;

typedef struct Ringer {
  bool active;
  bool currentOutput;
  bool lastOutput;
  int uptime;
  int downtime;
  long startTime;
  char remainingRings;
} Ringer;

//Global Variables
//Ringer r = { active, currentOutput, lastOutput, uptime, downtime, startTime, remainingRings};
Ringer r = { false, 0, false, 500, 1000, 0, 0};
Display d = { "", "", "", "" };
bool displayStatus = true;
Time t = { 88, 88, 88, 88, 88, 8888, 0, "" };
//Sensor s = { -52, 1125, false, 0, false, 0 };
Sensor s;

volatile bool getTimeFlag = true;
volatile bool printScreenFlag = true;
volatile bool getSensorFlag = true;

int setupMode = 0;
int setupCursor = 0;
/*
0 - Seconds
1 - Minutes
2 - Hours
3 - Day
4 - Month
5 - Year
6 - Weekday
*/
const uint8_t cursorPositions[7][2] = { { 3, 11 }, { 3, 8 }, { 3, 5 }, { 2, 6 }, { 2, 9 }, { 2, 14 }, { 2, 1 } };
const uint8_t daysInMonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

const int PIN_LCD_BACKLIGHT = A2;
const int PIN_RINGER = A3;
const int BUTTON0 = 7;
const int BUTTON1 = 5;
const int BUTTON2 = 4;
const int BUTTON3 = 3;
const int BUTTON4 = 2;
const long LONG_PRESS_TIME = 2000;

Button button0 = { BUTTON0, 0, 0, 0 };
Button button1_setup = { BUTTON1, 0, 0, 0 };
Button button2_select = { BUTTON2, 0, 0, 0 };
Button button3_minus = { BUTTON3, 0, 0, 0 };
Button button4_plus = { BUTTON4, 0, 0, 0 };

//Functions

void setTime() {
  rtc.setTime(t.hour, t.minute, t.second);
  rtc.setDate(t.weekdayIndex, t.day, t.month, t.year);
}

void getTime() {
  t.weekdayIndex = rtc.getWeekday();
  switch (t.weekdayIndex) {
    case 0:
      strcpy(t.weekday, "Sun");
      break;
    case 1:
      strcpy(t.weekday, "Mon");
      break;
    case 2:
      strcpy(t.weekday, "Tue");
      break;
    case 3:
      strcpy(t.weekday, "Wen");
      break;
    case 4:
      strcpy(t.weekday, "Thu");
      break;
    case 5:
      strcpy(t.weekday, "Fri");
      break;
    case 6:
      strcpy(t.weekday, "Sat");
      break;
  }
  t.year = rtc.getYear();
  t.month = rtc.getMonth();
  t.day = rtc.getDay();
  t.hour = rtc.getHour();
  t.minute = rtc.getMinute();
  t.second = rtc.getSecond();
}

void setPixel (int temperature) {
  if (temperature <= 15) temperature = 15;
  if (temperature >  30) temperature = 30;
  pixels.setPixelColor(0, pixels.Color(tempToRGB[temperature-15][0],tempToRGB[temperature-15][1],tempToRGB[temperature-15][2]));
  pixels.show();
}

void getSensor() {
  if (!s.tempStarted && !s.pressStarted) {
    // Start a temperature measurement
    bmp180.measureTemperature();
    s.tempStarted = true;
    s.tempLastTime = millis();
  }

  if (s.tempStarted && bmp180.hasValue()) {
    s.temperature = bmp180.getTemperature();
    s.tempStarted = false;
    setPixel(s.temperature);

    // Start a pressure measurement
    bmp180.measurePressure();
    s.pressStarted = true;
    s.pressLastTime = millis();
  }

  if (s.pressStarted && bmp180.hasValue()) {
    s.pressure = bmp180.getPressure();
    s.pressStarted = false;
  }
}

int debounce(Button *b) {
  int stateNow = digitalRead(b->pin);
  if (b->lastStatus != stateNow) {
    delay(10);
    stateNow = digitalRead(b->pin);
  }
  return stateNow;
}

bool checkButton(Button *b) {
  int button_state = debounce(b);
  //Button press down
  if (button_state == LOW && b->lastStatus == 0) {
    b->lastStatus = 1;
    b->longPress = 0;
    b->since = millis();
    digitalWrite(LED_BUILTIN, HIGH);
    return 0;
  }
  //Button still pressed, check for long press
  else if (button_state == LOW && b->lastStatus == 1) {
    digitalWrite(LED_BUILTIN, HIGH);
    if ((millis() - b->since) > LONG_PRESS_TIME) {
      b->longPress = 1;
    }
    return 0;
  }
  // Button release
  else if (button_state == HIGH && b->lastStatus == 1) {
    b->lastStatus = 0;
    digitalWrite(LED_BUILTIN, LOW);
    return 1;
  }
  return 0;
}

//----------------|
//-12°C  1000.21mb|
//                |
// Tue 30.05.2024 |
//    14:22:00    |
//----------------|
//Setup:          |
//                |
// Tue 30.05.2024 |
//    14:22:00    |
//----------------|

void printScreen() {

  //lcd.noBlink();

#ifdef DEBUG_VARS
  DEBUG_PRINT("Button0: %d\n", digitalRead(BUTTON0));
  DEBUG_PRINT("Button1: %d\n", digitalRead(BUTTON1));
  DEBUG_PRINT("Button2: %d\n", digitalRead(BUTTON2));
  DEBUG_PRINT("Button3: %d\n", digitalRead(BUTTON3));
  DEBUG_PRINT("Button4: %d\n", digitalRead(BUTTON4));
  DEBUG_PRINT("Temp: %d\n", s.temperature);
  DEBUG_PRINT("Pres: %f\n", s.pressure);
  DEBUG_PRINT("R:%d, G:%d, B:%d\n", tempToRGB[s.temperature][0],tempToRGB[s.temperature][1],tempToRGB[s.temperature][2]);
  DEBUG_PRINT("Ring Active:    %b\n", r.active);
  DEBUG_PRINT("Ring Current:   %b\n", r.currentOutput);
  DEBUG_PRINT("Ring Last:      %b\n", r.lastOutput);
  DEBUG_PRINT("Ring Remaining: %d\n", r.remainingRings);
  DEBUG_PRINT("Time: %02d:%02d:%02d %s %02d.%02d.%d\n", t.hour, t.minute, t.second, t.weekday, t.day, t.month, t.year);
#endif

  if (setupMode)
  // Setup Screen
  {
    sprintf(d.line0, "Setup:          ");
    sprintf(d.line2, " %s %02d.%02d.%d ", t.weekday, t.day, t.month, t.year);
    sprintf(d.line3, "    %02d:%02d:%02d    ", t.hour, t.minute, t.second);

    lcd.setCursor(0, 0);
    lcd.print(d.line0);
    //lcd.setCursor(0, 1);
    //lcd.print(d.line1);
    lcd.setCursor(0, 2);
    lcd.print(d.line2);
    lcd.setCursor(0, 3);
    lcd.print(d.line3);
    
    lcd.setCursor(cursorPositions[setupCursor][1], cursorPositions[setupCursor][0]);
    lcd.blink();

    #ifdef DEBUG_SCREEN
    DEBUG_PRINT("------------------\n");
    DEBUG_PRINT("|%s|\n", d.line0);
    DEBUG_PRINT("|                |\n");
    DEBUG_PRINT("|%s|\n", d.line2);
    DEBUG_PRINT("|%s|\n", d.line3);
    #endif
  } else
  // Clock Screen
  {
    sprintf(d.line0, "%3d%cC % 8.2fmb", s.temperature, 223, s.pressure / 100);  //char°223
    sprintf(d.line2, " %s %02d.%02d.%d ", t.weekday, t.day, t.month, t.year);
    sprintf(d.line3, "    %02d:%02d:%02d    ", t.hour, t.minute, t.second);

    lcd.setCursor(0, 0);
    lcd.print(d.line0);
    //lcd.setCursor(0, 1);
    //lcd.print(d.line1);
    lcd.setCursor(0, 2);
    lcd.print(d.line2);
    lcd.setCursor(0, 3);
    lcd.print(d.line3);

    #ifdef DEBUG_SCREEN
    DEBUG_PRINT("------------------\n");
    DEBUG_PRINT("|%s|\n", d.line0);
    DEBUG_PRINT("|                |\n");
    DEBUG_PRINT("|%s|\n", d.line2);
    DEBUG_PRINT("|%s|\n", d.line3);
    #endif
  }
}

void increaseDigit() {
  switch (setupCursor) {
    case 0:
      if (++t.second > 59) t.second = 0;
      break;
    case 1:
      if (++t.minute > 59) t.minute = 0;
      break;
    case 2:
      if (++t.hour > 23) t.hour = 0;
      break;
    case 3:
      if (++t.day > daysInMonth[t.month-1]) t.day = 1;
      break;
    case 4:
      if (++t.month > 12) t.month = 1;
      break;
    case 5:
      t.year++;
      break;
    case 6:
      if (++t.weekdayIndex > 6) t.weekdayIndex = 0;
      break;
  }
  setTime();
  getTime();
  printScreenFlag = true;
}

void decreaseDigit() {
  switch (setupCursor) {
    case 0:
      if (--t.second == 255) t.second = 59;
      break;
    case 1:
      if (--t.minute == 255) t.minute = 59;
      break;
    case 2:
      if (--t.hour == 255) t.hour = 23;
      break;
    case 3:
      if (--t.day == 0) t.day = daysInMonth[t.month-1];
      break;
    case 4:
      if (--t.month == 1) t.month = 12;
      break;
    case 5:
      t.year--;
      break;
    case 6:
      if (--t.weekdayIndex == 255) t.weekdayIndex = 6;
      break;
  }
  setTime();
  getTime();
  printScreenFlag = true;
}

// TODO
void error(char errorNumber) {
  do {
    for (char i=0; i < errorNumber; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(30);
    }
    delay(970);
  } while (true);
}

bool toggleDisplay(){
  if (displayStatus == true) {
      lcd.noDisplay();
      analogWrite(A2, 0);
      displayStatus = false;
  }
  else {
    lcd.display();
    analogWrite(A2, 255);
    displayStatus = true;
  }
}

void updateRinger() {
  //Ring start
  if (r.currentOutput == 0  && !r.lastOutput ) {
    analogWrite(PIN_RINGER, 255);
    r.currentOutput = true;
    r.lastOutput = true;
    r.startTime = millis();
    return;
  }
  //Still ringing
  if (r.currentOutput && r.lastOutput) {
    if (millis() - r.startTime > r.uptime) {
      analogWrite(PIN_RINGER, 0);
      r.currentOutput = false;
    }
    return;
  }
  //Ringer silence period
  if (!r.currentOutput && r.lastOutput ) {
    if ((millis() - r.startTime - r.uptime) > r.downtime) {
      analogWrite(PIN_RINGER, 0);
      r.lastOutput = false;
      if (--r.remainingRings == 0) {
        r.active = false;
      }
    }
    return;
  }
}

void startRinger(uint8_t rings) {
    r.remainingRings = rings; //t.hour % 12;
    r.active = true;
}

void intro() {
    sprintf(d.line0, "  \"Bell Clock\"  ");
    lcd.setCursor(0, 1);
    lcd.print(d.line0);
    sprintf(d.line0, "     by Luka    ");
    lcd.setCursor(0, 2);
    lcd.print(d.line0);
    for (int j=0; j<2; j++) {
      for (int i=7; i>=0; i--) {
        lcd.setCursor(i, 0);
        lcd.print("+");
        lcd.setCursor(15-i, 0);
        lcd.print("+");

        lcd.setCursor(i, 3);
        lcd.print("+");
        lcd.setCursor(15-i, 3);
        lcd.print("+");

        delay(250);
      }
      for (int i=7; i>=0; i--) {
        lcd.setCursor(i, 0);
        lcd.print(" ");
        lcd.setCursor(15-i, 0);
        lcd.print(" ");

        lcd.setCursor(i, 3);
        lcd.print(" ");
        lcd.setCursor(15-i, 3);
        lcd.print(" ");
        delay(250);
      }
    }
    lcd.setCursor(0, 1);
    lcd.print("                ");    
}

//TIMERS
ISR_Timer ISR_timer;

void timerHandler() {
  ISR_timer.run();
}

void timerRefreshScreen() {
  printScreenFlag = true;
}

void timer_1sec() {
  getTimeFlag = true;
  getSensorFlag = true;
}

// Setup
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(LED_BUILTIN, HIGH);
  pinMode(BUTTON0, INPUT_PULLUP);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON4, INPUT_PULLUP);
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  pinMode(PIN_RINGER, OUTPUT);
  pinMode(7, INPUT_PULLUP);
  analogWrite(PIN_LCD_BACKLIGHT, 255); //Turn on Display
  analogWrite(PIN_RINGER, 0); //Turn off Ringer

  #ifdef DEBUG_PRINT
  Serial.begin(115200);
  #endif

  //For I2C library .... TOBUY
  //lcd.init();
  //lcd.backlight();
  lcd.begin(LCD_COLONS, LCD_ROWS); 

  rtc.begin();
  getTime();

  bmp180.begin();
  bmp180.resetToDefaults();
  bmp180.setSamplingMode(BMP180MI::MODE_UHR);

  ITimer1.init();
  ITimer1.attachInterruptInterval(50, timerHandler);
  ISR_timer.setInterval(1000, timerRefreshScreen);
  ISR_timer.setInterval(1000, timer_1sec);

  // Set 24h mode on RTC
  Wire.beginTransmission(0x51);
  Wire.write(0x0);  // Select address of Control_1 register
  Wire.write(0x0);  // Set all bits to 0
  Wire.endTransmission();
  //rtc.reset();
  //rtc.setTime(17, 6, 30); // 24H mode, ex. 6:54:00
  //rtc.setDate(0, 2, 6, 2024); // 0 for Sunday, ex. Saturday, 16.5.2020.
  while (s.temperature == 0 || s.pressure == 0.0) {
    getSensor();
  }

  pixels.begin();
  pixels.setBrightness(32);
  pixels.clear();
  pixels.show();
  intro();
}

// Main loop

void loop() {
  if (getTimeFlag) {
    if (setupMode) {
      if (++t.second > 59) t.second = 0;
    } else {
      getTime();
      if (t.minute == 0 && t.second == 0) {
        startRinger(t.hour % 12);
        if (t.hour % 12 == 0) startRinger(12);
      }
      //Check for Ring
    }
    getTimeFlag = false;
  }

  if (getSensorFlag) {
    getSensor();
    getSensorFlag = false;
  }

  if (r.active) {
    updateRinger();
    //ringerHandles up
  }

  if (printScreenFlag && displayStatus) {
    printScreen();
    printScreenFlag = false;
  }

  // Handle buttons

  if (setupMode) {
    // Button1 - Leave setup and update Time/Date
    if (checkButton(&button1_setup)) {
      setupMode = false;
    }
    // Button2 - Select
    if (checkButton(&button2_select)) {
      if (++setupCursor > 6) setupCursor = 0;
    }
    // Button3 - Plus
    if (checkButton(&button3_minus)) {
      decreaseDigit();
    }
    // Button4 - Minus
    if (checkButton(&button4_plus)) {
      increaseDigit();
    }
  } else {
    if (checkButton(&button1_setup)) {
      if (button1_setup.longPress == 1 && displayStatus == true) {
        setupMode = true;
        setupCursor = 0;  //Seconds
      }
      else {
        toggleDisplay();
      }
    }

    if (checkButton(&button2_select)) {
      toggleDisplay();
    }

    if (checkButton(&button3_minus)) {
      toggleDisplay();
    }

    if (checkButton(&button4_plus)) {
      // TODO toggle Ringer
      //toggleDisplay();
      startRinger(++r.remainingRings);
      DEBUG_PRINT("RING!!!\n");
    }
  }

  // TODO Handle BackLight Idle
  // TODO Automatic Backlight
}
