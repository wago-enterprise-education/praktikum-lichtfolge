#include <Bounce2.h> // Library for debouncing buttons
#include <EEPROM.h> // Use EEPROM to store highscores

#define EEPROM_INIT_ADDRESS 0 // EEPROM address for the init marker, to set the EEPROM to 0 at first boot
#define EEPROM_INIT_MARKER 0x42
#define EEPROM_START_ADDRESS 1 // Start address to store the highscores (first address after init marker)

// All timing values in ms
#define RESET_BUTTON_PRESS_DURATION 3000 // Nesscesary time to hold the reset button

#define SIMON_SAYS_START_ON_TIME 400 // On time of the LEDs for the first sequence
#define SIMON_SAYS_PAUSE_TIME 150 // LED off time before the next LED in the sequence lights up to allow the same LED to light up again
#define SIMON_SAYS_DECREMENT_TIME 10 // Time decrement after each sequence
#define SIMON_SAYS_MIN_ON_TIME 250 // Minmum LED on time
#define SIMON_SAYS_SEQ_LEN 64 // Max sequence length
#define SIMON_SAYS_CHECK_LED_ON_TIME 450 // LED on time as feedback when the user enters the sequence

#define WHACK_A_MOLE_GAME_DURATION 30000

// Pin assignments (see Pinout_x4.jpg)
uint8_t leds[] = {PIN_PB1, PIN_PB0, PIN_PA7, PIN_PB2};
uint8_t buttonPins[] = {PIN_PA1, PIN_PA0, PIN_PA3, PIN_PA2};

/*
// Breadboard setup
uint8_t leds[] = {PIN_PA7, PIN_PB2, PIN_PB1, PIN_PB0};
uint8_t buttonPins[] = {PIN_PA3, PIN_PA2, PIN_PA1, PIN_PA0};
*/

Bounce2::Button buttons[4]; // Create 4 button objects

enum Buttons {GAME, RESET, HIGHSCORE, START};
enum Game {SIMON_SAYS, WHACK_A_MOLE, ROLL_DICE, GAME4};

Game selectedGame; // Global variable to store currently selected game

void setup() {
  initEEPROM(); // Reset EEPROM on first start

  // Seed the pseudo random number generator 
  pinMode(A1, INPUT); // Use ADC1 which is also PIN_PA1 where the first button is connected to since there is no unused pin left
  delay(100);
  uint16_t seedVal = analogRead(A1);
  while(seedVal == 0) { // Don't seed the pseudo random number generator as long as the button is pressed to prevent cheating
    delay(100);
    seedVal = analogRead(A1);
  }
  randomSeed(seedVal);

  // Setup the LEDs and buttons
  for(uint8_t i = 0; i < 4; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);

    buttons[i] = Bounce2::Button();
    buttons[i].attach(buttonPins[i], INPUT_PULLUP); // INPUT_PULLUP for a defined state when the button is not beeing pressed
    buttons[i].interval(25); // Debounce interval. Physical buttons and switches bounce when they are pessed, filter these undesired state changes.
    buttons[i].setPressedState(LOW); // Pin is connected to GND (LOW) on the pcb when button is pressed
  }

  selectedGame = static_cast<Game>(0); // Always select the first game on startup. Could be changed and read from EEPROM to remember last game before switching off.

  startupAnimation();
}

void loop() {
  showMenu();
}

void showMenu() {
  updateButtons(); // Update button states. This must be called repetately.
  displayGame(selectedGame); // Display selected game using the corresponding LED

  if(buttons[GAME].pressed()) selectGame();
  else if(buttons[RESET].pressed()) resetHighscore(selectedGame);
  else if(buttons[HIGHSCORE].pressed()) displayHighscore(selectedGame);
  else if(buttons[START].pressed()) startGame(selectedGame);
}

void displayGame(Game game) {
  for(uint8_t i = 0; i < 4; i++) {
    digitalWrite(leds[i], (i == game) ? HIGH : LOW); // LED of corresponding LED HIGH if it's the selected game, otherwise LOW
  }
}

void selectGame() {
  selectedGame = static_cast<Game>((selectedGame + 1) % 4); // Select next game and go back to 0 if selectedGame > 3
}

void resetHighscore(Game game) {
  while(buttons[RESET].isPressed()) {
    buttons[RESET].update();
    if(buttons[RESET].currentDuration() >= RESET_BUTTON_PRESS_DURATION) {
      errorAnimation();
      clearHighscore(game);
      return; // If reset button is beeing pressed long enough, clear highscore in EEPROM and return to menu
    }
  }
}

void clearHighscore(Game game) {
  uint16_t address = EEPROM_START_ADDRESS + game * sizeof(uint16_t);
  EEPROM.put(address, 0);
}

uint16_t getHighscore(Game game) {
  uint16_t highscore;
  uint16_t address = EEPROM_START_ADDRESS + game * sizeof(uint16_t); 
  EEPROM.get(address, highscore);
  return highscore;
}

void updateHighscore(Game game, uint16_t score) {
  if(score > getHighscore(game)) { // Check if score is higher than current highscore
    if (score > 9999) { // Only scores up to 9999 can be displayed, if a score is higher, set to 9999
    score = 9999;
    }
    uint16_t address = EEPROM_START_ADDRESS + game * sizeof(uint16_t); // EEPROM.write() can only save 8 bits (1 Byte). Calculate the correct address.
    EEPROM.put(address, score); // Use EEPROM.put() to write any datatype 
  }
}

void displayHighscore(Game game) {
  uint16_t highscore = getHighscore(game);
  if(highscore != 0) displayScore(highscore);
  else errorAnimation();
}

void displayScore(uint16_t score) {
  clearLEDs();
  delay(250);
  for (uint8_t i = 0; i < 4; i++) { // Each of the four LEDs represent a digit 
    uint8_t digit = (score / (uint16_t)pow(10, 3-i)) % 10;
    for (uint8_t j = 0; j < digit; j++) { // Blink that LED according to the current digit of the score
      digitalWrite(leds[i], HIGH);
      delay(250);
      digitalWrite(leds[i], LOW);
      delay(250);
    }
  }
}

void startGame(Game game) {
  updateButtons();
  clearLEDs();
  switch (game) {
    case SIMON_SAYS:
      playSimonSays();
      break;
    case WHACK_A_MOLE:
      playWhackAMole();
      break;
    case ROLL_DICE:
      playRollDice();
      break;
    case GAME4:
      playGame4();
      break;
  }
  showMenu();
}

void playSimonSays() { // Repeat a given random sequence
  bool success = true;
  uint8_t seq[SIMON_SAYS_SEQ_LEN]; // Array to store the sequence
  uint8_t seqLen = 0; // and a variable to store the current length
  uint16_t onTime = SIMON_SAYS_START_ON_TIME;

  startupAnimation();
  delay(500);

  while(success) { // As long as the sequence is correct
    seq[seqLen] = random(4); // add a random LED (0 - 3)
    seqLen++;

    displaySequence(seq, seqLen, onTime);
    if(onTime - SIMON_SAYS_DECREMENT_TIME >= SIMON_SAYS_MIN_ON_TIME) onTime -= SIMON_SAYS_DECREMENT_TIME; // Sequence gets faster each round
    else onTime = SIMON_SAYS_MIN_ON_TIME;

    success = checkSequence(seq, seqLen); // User inputs the sequence
    if(success) {
      if(seqLen == SIMON_SAYS_SEQ_LEN) { // Stop if maxium length is reached
        seqLen++;
        break;
      }
      delay(SIMON_SAYS_CHECK_LED_ON_TIME);
      clearLEDs();
      delay(250);
    }
  }
  errorAnimation();
  delay(250);
  updateHighscore(SIMON_SAYS, seqLen - 1);
  displayScore(seqLen - 1);
  delay(500);
  startupAnimation();
}

void displaySequence(uint8_t seq[], uint8_t seqLen, uint16_t onTime) { // Display the sequence
  clearLEDs();
  for(uint8_t i = 0; i < seqLen; i++) {
    delay(SIMON_SAYS_PAUSE_TIME);
    digitalWrite(leds[seq[i]], HIGH);
    delay(onTime);
    digitalWrite(leds[seq[i]], LOW);
  }
}

bool checkSequence(uint8_t seq[], uint8_t seqLen) { // Check if the user correctly inputs the sequence
  uint8_t seqPos = 0;
  bool seqCorrect = true;
  uint32_t ledStartTime; // Variable to store the time the corresponding LED lit up as feedback for the user

  while(seqCorrect && seqPos < seqLen) {
    updateButtons();

    for (uint8_t i = 0; i < 4; i++) { // Check each button state
      if(buttons[i].pressed()) {
        clearLEDs(); // Also turn off last LED when a new button is pressed
        digitalWrite(leds[i], HIGH);
        ledStartTime = millis();
        if(seq[seqPos] == i) seqPos++; // If the pressed button was correct, go to next position in the sequence
        else seqCorrect = false;
      }
    }
    if(millis() - ledStartTime > SIMON_SAYS_CHECK_LED_ON_TIME) clearLEDs(); // Turn off LED time based
  }
  return seqCorrect;
}

void playWhackAMole() { // Click the correct button as fast as possible
  uint32_t startTime = millis();
  uint16_t score = 0;
  uint8_t currentLED = random(4); // Start with a random LED

  startupAnimation();
  delay(500);

  while (millis() - startTime < WHACK_A_MOLE_GAME_DURATION) { // Stop the game when time is over
    digitalWrite(leds[currentLED], HIGH); // Light up the current LED

    updateButtons();

    for (uint8_t i = 0; i < 4; i++) { // Check all buttons
      if (buttons[i].pressed()) {
        if (i == currentLED) {
          score++;  // Increment score for the correct button press
          digitalWrite(leds[currentLED], LOW);  // Turn off the current LED

          // Select the next LED excluding the current one
          uint8_t nextLED = random(4);
          while (nextLED == currentLED) {
            nextLED = random(4);
          }
          currentLED = nextLED;
        } 
        else {
          // Decrease score for incorrect button press unless zero
          score = (score > 0) ? score - 1 : 0;
        }
      }
    }
    
    if(buttons[RESET].isPressed() && buttons[RESET].currentDuration() > 5000) break; // Quit the game by holding the reset button
  }

  // Display the score and update highscore after the game ends
  clearLEDs();
  errorAnimation();
  delay(250);
  updateHighscore(WHACK_A_MOLE, score);
  displayScore(score);
  delay(500);
  startupAnimation();
}

void playRollDice() { // Show a random number from 1 to 6
  uint8_t number = random(6) + 1;
  randomAnimation(ROLL_DICE, 15, 50, 1.15); // Animation to randomly light up LEDs with decreasing speed
  delay(400);
  for(uint8_t i = 0; i < number; i++) { // Turn all LEDs on and off
    for(uint8_t j = 0; j < 4; j++) {
      digitalWrite(leds[j], HIGH);
    }
    delay(250);
    for(uint8_t j = 0; j < 4; j++) {
      digitalWrite(leds[j], LOW);
    }
    delay(250);
  }
  delay(250);
}

void playGame4() {
  errorAnimation();
  // Put your game logic here
}

void updateButtons() { // Update state of all buttons
  for (uint8_t i = 0; i < 4; i++) {
    buttons[i].update();
  }
}

void clearLEDs() { // Clear all LEDs
  for(uint8_t i = 0; i < 4; i++) {
    digitalWrite(leds[i], LOW);
  }
}

void startupAnimation() { // Show a simple animation to make the UI more intuitive
  for(uint8_t i = 0; i < 4; i++) {
    digitalWrite(leds[i], HIGH);
    delay(150);
    digitalWrite(leds[i], LOW);
  }
  for(int8_t i = 2; i >= 0; i--) {
    digitalWrite(leds[i], HIGH);
    delay(150);
    digitalWrite(leds[i], LOW);
  }
}

void errorAnimation() {
  for(uint8_t i = 0; i < 4; i++) {
    for(uint8_t j = 0; j < 4; j++) {
      digitalWrite(leds[j], HIGH);
    }
    delay(100);
    for(uint8_t j = 0; j < 4; j++) {
      digitalWrite(leds[j], LOW);
    }
    delay(100);
  }
}

void randomAnimation(uint8_t currentLED, uint8_t num, uint16_t T_start, float decFactor) { 
  uint8_t lastLED = currentLED; // First LED to light up is not the current LED
  uint8_t nextLED = random(4);
  clearLEDs();

  for(uint8_t i = 0; i < num; i++) {
    while(nextLED == lastLED) {
      nextLED = random(4);
    }
    digitalWrite(leds[lastLED], LOW);
    digitalWrite(leds[nextLED], HIGH);
    lastLED = nextLED;

    // Exponentially slow down animation
    uint16_t delayTime = T_start * pow(decFactor, i);
    
    delay(delayTime);
  }
  delay(250);
  clearLEDs();
}

void initEEPROM() {
  if (EEPROM.read(EEPROM_INIT_ADDRESS) != EEPROM_INIT_MARKER) { // If init marker has not been set the microcontroller is new and contains random values in EEPROM
    resetEEPROM(); // Reset EEPROM
    EEPROM.write(EEPROM_INIT_ADDRESS, EEPROM_INIT_MARKER); // Set init marker
  }
}

void resetEEPROM() {
  for (uint16_t i = 0 ; i < EEPROM.length() ; i++) { // Set all bytes in EEPROM to 0
    EEPROM.write(i, 0);
  }
}