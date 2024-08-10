#include <Wire.h>
#include <Keypad.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFPlayerMini_Fast.h>

#define PIN 5
#define NUMPIXELS 42
#define ROWS 7
#define COLUMNS 6

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS_KEYPAD = 2;
const byte COLUMNS_KEYPAD = 3;
char keys[ROWS_KEYPAD][COLUMNS_KEYPAD] = {
  {'1', '2', '3'},
  {'4', '5', '6'}
};
byte rowPins[ROWS_KEYPAD] = {13, 12};
byte colPins[COLUMNS_KEYPAD] = {9, 8, 7};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS_KEYPAD, COLUMNS_KEYPAD);

const int buttonYellowPin = 2;
const int buttonBluePin = 3;
const int switchPin = 4;

SoftwareSerial mySerial(10, 11);
DFPlayerMini_Fast myMP3;

volatile bool changeToYellow = false;
volatile bool changeToBlue = false;
volatile unsigned long lastInterruptTimeYellow = 0;
volatile unsigned long lastInterruptTimeBlue = 0;
const unsigned long debounceDelay = 200;

int columnState[COLUMNS] = {0, 0, 0, 0, 0, 0}; 
int grid[ROWS][COLUMNS] = {0};
int selectedColumn = -1;
bool gameActive = false;
bool gameOver = false;
int currentPlayer = 1;
bool wasSwitchOn = false;

void setup() {
  pinMode(switchPin, INPUT_PULLUP); 

  Serial.begin(9600);
  while (digitalRead(switchPin) == HIGH) {
  }

  pixels.begin();

  mySerial.begin(9600);
  myMP3.begin(mySerial);
  myMP3.volume(30);
  myMP3.loop(1); 
  Serial.println(F("DFPlayer Mini online."));

  lcd.init();
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Welcome to");
  lcd.setCursor(2, 1);
  lcd.print("Find Your 4!");
  delay(3000); 

  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Select Mode:");
  lcd.setCursor(4, 1);
  lcd.print("1.Normal");

  pinMode(buttonYellowPin, INPUT_PULLUP);
  pinMode(buttonBluePin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(buttonYellowPin), buttonYellowISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonBluePin), buttonBlueISR, FALLING);

  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
  Serial.println("Setup Complete");
}

void loop() {
  
  bool isSwitchOn = digitalRead(switchPin) == LOW;

  if (isSwitchOn && !wasSwitchOn) {
    gameActive = false;
    resetGame();
    Serial.println("Game Reset");
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Select Mode:");
    lcd.setCursor(4, 1);
    lcd.print("1.Normal");
    if (!myMP3.isPlaying()) {
      myMP3.loop(1);
    }
  }

  wasSwitchOn = isSwitchOn; 

  if (!gameActive) {
    char mode = keypad.getKey();
    if (mode == '1') {
      displayMode("Normal Mode");
      startGame();
    }
  } else {
    if (isSwitchOn) {
      if (!gameActive) {
        gameActive = true;
        gameOver = false;
        resetGame();
        Serial.println("Game Started");
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("Game Started");
      }
    } else {
      if (gameActive) {
        gameActive = false;
        Serial.println("Game Stopped");
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("Game Stopped");
      }
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.show();
      return;
    }

    if (gameActive && !gameOver) {

      char key = keypad.getKey();

      if (key) {
        Serial.println(key);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Key: ");
        lcd.print(key);
        myMP3.play(3); 

        if (key >= '1' && key <= '6') {
          selectedColumn = key - '1';
          Serial.print("Column Selected: ");
          Serial.println(selectedColumn);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Column: ");
          lcd.print(selectedColumn + 1);

          if (columnState[selectedColumn] < ROWS) {
            int ledIndex = columnState[selectedColumn] * COLUMNS + selectedColumn;
            pixels.setPixelColor(ledIndex, pixels.Color(255, 255, 255));
            pixels.show();
            Serial.print("LED index lit: ");
            Serial.println(ledIndex);
          } else {
            Serial.println("Column is full");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Column is full");
          }
        }
      }

      if (changeToYellow && selectedColumn != -1) {
        Serial.println("Changing to Yellow");
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Turning Yellow");
        myMP3.play(4);
        int ledIndex = columnState[selectedColumn] * COLUMNS + selectedColumn;
        pixels.setPixelColor(ledIndex, pixels.Color(255, 255, 0));
        pixels.show();
        grid[columnState[selectedColumn]][selectedColumn] = 1;
        columnState[selectedColumn]++;
        selectedColumn = -1;
        changeToYellow = false;

        if (checkWin(1)) {
          Serial.println("Yellow wins!");
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("Yellow wins!");
          gameOver = true;
          myMP3.play(5); 
          flashWinnerColor(255, 255, 0);
        } else if (isDraw()) {
          Serial.println("Game is a draw!");
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("It's a Draw!");
          gameOver = true;
          myMP3.play(7); 
          flashWinnerColor(255, 255, 255); 
        }
      }

      if (changeToBlue && selectedColumn != -1) {
        Serial.println("Changing to Blue");
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("Turning Blue");
        myMP3.play(4); 
        int ledIndex = columnState[selectedColumn] * COLUMNS + selectedColumn;
        pixels.setPixelColor(ledIndex, pixels.Color(0, 0, 255));
        pixels.show();
        grid[columnState[selectedColumn]][selectedColumn] = 2;
        columnState[selectedColumn]++;
        selectedColumn = -1;
        changeToBlue = false;

        if (checkWin(2)) {
          Serial.println("Blue wins!");
          lcd.clear();
          lcd.setCursor(3, 0);
          lcd.print("Blue wins!");
          gameOver = true;
          myMP3.play(6); 
          flashWinnerColor(0, 0, 255);
        } else if (isDraw()) {
          Serial.println("Game is a draw!");
          lcd.clear();
          lcd.setCursor(2, 0);
          lcd.print("It's a draw!");
          gameOver = true;
          myMP3.play(7); 
          flashWinnerColor(255, 255, 255); 
        }
      }
    }
  }
}

void buttonYellowISR() {
  unsigned long currentInterruptTime = millis();
  if (currentInterruptTime - lastInterruptTimeYellow > debounceDelay) {
    if (selectedColumn != -1 && !changeToYellow && !changeToBlue) {
      changeToYellow = true;
      lastInterruptTimeYellow = currentInterruptTime;
    }
  }
}

void buttonBlueISR() {
  unsigned long currentInterruptTime = millis();
  if (currentInterruptTime - lastInterruptTimeBlue > debounceDelay) {
    if (selectedColumn != -1 && !changeToBlue && !changeToYellow) {
      changeToBlue = true;
      lastInterruptTimeBlue = currentInterruptTime;
    }
  }
}

void displayMode(String mode) {
  myMP3.stop();
  myMP3.play(2);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(mode);
  delay(2000);
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Ready...");
  delay(1000);
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Set...");
  delay(1000);
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Stack!");
  delay(1000);
}

void startGame() {
  gameActive = true;
  gameOver = false;
  resetGame();
}

void resetGame() {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLUMNS; j++) {
      grid[i][j] = 0;
    }
  }
  for (int i = 0; i < COLUMNS; i++) {
    columnState[i] = 0;
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
  selectedColumn = -1;
  currentPlayer = 1;
}

bool checkWin(int player) {
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLUMNS; col++) {
      if (grid[row][col] == player) {
        if (col + 3 < COLUMNS && grid[row][col + 1] == player && grid[row][col + 2] == player && grid[row][col + 3] == player) {
          return true;
        }
        if (row + 3 < ROWS) {
          if (grid[row + 1][col] == player && grid[row + 2][col] == player && grid[row + 3][col] == player) {
            return true;
          }
          if (col + 3 < COLUMNS && grid[row + 1][col + 1] == player && grid[row + 2][col + 2] == player && grid[row + 3][col + 3] == player) {
            return true;
          }
          if (col - 3 >= 0 && grid[row + 1][col - 1] == player && grid[row + 2][col - 2] == player && grid[row + 3][col - 3] == player) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool isDraw() {
  for (int col = 0; col < COLUMNS; col++) {
    if (columnState[col] < ROWS) {
      return false;
    }
  }
  return true;
}

void flashWinnerColor(int r, int g, int b) {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < NUMPIXELS; j++) {
      pixels.setPixelColor(j, pixels.Color(r, g, b));
    }
    pixels.show();
    delay(500);
    for (int j = 0; j < NUMPIXELS; j++) {
      pixels.setPixelColor(j, pixels.Color(0, 0, 0));
    }
    pixels.show();
    delay(500);
  }
}
