#include <M5Cardputer.h>

// ---------------- DATA ----------------
const int diceTypes[] = {4, 6, 8, 10, 12, 20, 100};

String input = "";
int selectedDieIndex = -1;
int numDice = 1;

enum ScreenState { MENU, NUM_INPUT, RESULT, DICE_VIEW };
ScreenState state = MENU;

// ---- result data ----
String resultHeader = "";
String resultRolls = "";

// ---- dice view (scrolling) ----
String viewLines[260];
int viewLineCount = 0;
int viewScroll = 0;

// ------------- FORWARD DECLS -------------
void drawMenu();
void drawNumInput();
void drawResult();
void drawDiceView();
void computeViewLinesFromRolls();
void rollDice();
void resetToMenu();

// ------------- KEY HELPERS -------------
static bool isUpKey(char c)   { return c == ';'; } // UP
static bool isDownKey(char c) { return c == '.'; } // DOWN
static bool isShowKey(char c) { return c == 's' || c == 'S'; }

// ---------------- LAYOUT CONSTANTS ----------------
// Result header box tuned for textSize=2
const int RESULT_BOX_X = 10;
const int RESULT_BOX_Y = 10;
const int RESULT_BOX_W = 220;
const int RESULT_BOX_H = 40;

// Dice-view layout tuned to keep footer visible on 240x135
const int DICE_BOX_X = 10;
const int DICE_BOX_Y = 26;   // below title
const int DICE_BOX_W = 220;
const int DICE_BOX_H = 60;   // footer fits below
const int DICE_BOX_PAD_X = 8;
const int DICE_BOX_PAD_Y = 8;

// Text sizing assumptions
const int LINE_H_2X = 16;    // approx line height at textSize=2

int diceLinesVisible() {
  int innerH = DICE_BOX_H - 2 * DICE_BOX_PAD_Y;
  int n = innerH / LINE_H_2X;
  if (n < 1) n = 1;
  return n;
}

// ---------------- SETUP ----------------
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(160);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.fillScreen(BLACK);

  randomSeed(esp_random());
  drawMenu();
}

// ---------------- LOOP ----------------
void loop() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange()) return;

  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

  // SPACE = MENU (works from anywhere)
  for (auto c : status.word) {
    if (c == ' ') {
      resetToMenu();
      return;
    }
  }

  // Scroll + show/hide toggles should work on change (not only pressed)
  if (state == RESULT || state == DICE_VIEW) {
    for (auto c : status.word) {
      if (isUpKey(c)) {
        if (viewScroll > 0) viewScroll--;
        if (state == DICE_VIEW) drawDiceView();
        else drawResult();
        return;
      }
      if (isDownKey(c)) {
        int visible = diceLinesVisible();
        int maxScroll = viewLineCount - visible;
        if (maxScroll < 0) maxScroll = 0;
        if (viewScroll < maxScroll) viewScroll++;
        if (state == DICE_VIEW) drawDiceView();
        else drawResult();
        return;
      }
      if (isShowKey(c)) {
        if (state == RESULT) {
          state = DICE_VIEW;
          drawDiceView();
        } else if (state == DICE_VIEW) {
          state = RESULT;
          drawResult();
        }
        return;
      }
    }
  }

  // From here on: only handle normal input on key press
  if (!M5Cardputer.Keyboard.isPressed()) return;

  // BACKSPACE
  if (status.del) {
    if (input.length() > 0) {
      input.remove(input.length() - 1);
      if (state == MENU) drawMenu();
      else if (state == NUM_INPUT) drawNumInput();
    }
    return;
  }

  // ENTER
  if (status.enter) {
    if (state == MENU) {
      int v = input.toInt();

      // Only allow 1..7. Anything else acts like Space (reset).
      if (v < 1 || v > 7) {
        resetToMenu();
        return;
      }

      selectedDieIndex = v - 1;
      input = "";
      state = NUM_INPUT;
      drawNumInput();
    }
    else if (state == NUM_INPUT) {
      int v = input.length() ? input.toInt() : 1; // blank -> 1
      numDice = constrain(v, 1, 99);
      input = "";
      rollDice();
    }
    else if (state == RESULT || state == DICE_VIEW) {
      rollDice();
    }
    return;
  }

  // DIGITS
  for (auto c : status.word) {
    if (!isDigit((uint8_t)c)) continue;

    if (state == MENU) {
      // Only allow 1..7 in menu. Anything else acts like Space (reset).
      if (c < '1' || c > '7') {
        resetToMenu();
        return;
      }

      // Only accept one digit for menu selection
      if (input.length() == 0) {
        input += c;
        drawMenu();
      }
    }
    else if (state == NUM_INPUT) {
      if (input.length() < 2) {
        input += c;
        drawNumInput();
      }
    }
  }
}

// ---------------- DRAWING ----------------
void drawMenu() {
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(GREEN);

  M5Cardputer.Display.println("D&D Dice Roller");
  M5Cardputer.Display.println();
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.println("Choose a die:");
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.println("1 = d4   2 = d6");
  M5Cardputer.Display.println("3 = d8   4 = d10");
  M5Cardputer.Display.println("5 = d12  6 = d20");
  M5Cardputer.Display.println("7 = d100");
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.print("> ");
  M5Cardputer.Display.print(input);
}

void drawNumInput() {
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(GREEN);

  if (selectedDieIndex >= 0 && selectedDieIndex <= 6) {
    M5Cardputer.Display.printf("Selected: D%d\n", diceTypes[selectedDieIndex]);
  } else {
    M5Cardputer.Display.println("Selected: (none)");
  }
  M5Cardputer.Display.println();
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.println("How many?");
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.println("(1-99)");
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.print("> ");
  M5Cardputer.Display.print(input);
  M5Cardputer.Display.println();
  M5Cardputer.Display.println();
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.println("Enter = roll");
  M5Cardputer.Display.println("Space = menu");
}

void drawResult() {
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.drawRect(RESULT_BOX_X, RESULT_BOX_Y, RESULT_BOX_W, RESULT_BOX_H, YELLOW);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(RED);
  M5Cardputer.Display.setCursor(RESULT_BOX_X + 10, RESULT_BOX_Y + 12);
  M5Cardputer.Display.print(resultHeader);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.setCursor(0, RESULT_BOX_Y + RESULT_BOX_H + 6);
  M5Cardputer.Display.println();
  M5Cardputer.Display.println("Enter = reroll");
  M5Cardputer.Display.println("S = show dice");
  M5Cardputer.Display.println("Space = menu");
}

void drawDiceView() {
  M5Cardputer.Display.fillScreen(BLACK);

  // Title
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(GREEN);
  M5Cardputer.Display.println("Dice Rolls");

  // Dice rectangle
  M5Cardputer.Display.drawRect(DICE_BOX_X, DICE_BOX_Y, DICE_BOX_W, DICE_BOX_H, YELLOW);

  int innerX = DICE_BOX_X + DICE_BOX_PAD_X;
  int innerY = DICE_BOX_Y + DICE_BOX_PAD_Y;
  int visible = diceLinesVisible();

  // Clamp scroll
  int maxScroll = viewLineCount - visible;
  if (maxScroll < 0) maxScroll = 0;
  if (viewScroll < 0) viewScroll = 0;
  if (viewScroll > maxScroll) viewScroll = maxScroll;

  // Print inside box only
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(RED);

  for (int i = 0; i < visible; i++) {
    int idx = viewScroll + i;
    if (idx >= viewLineCount) break;
    M5Cardputer.Display.setCursor(innerX, innerY + i * LINE_H_2X);
    M5Cardputer.Display.print(viewLines[idx]);
  }

  // ▲ / ▼ indicators inside the box (top-right / bottom-right)
  int innerW = DICE_BOX_W - 2 * DICE_BOX_PAD_X;
  int arrowX = innerX + innerW - 10;
  M5Cardputer.Display.setTextColor(YELLOW);

  if (viewScroll > 0) {
    M5Cardputer.Display.setCursor(arrowX, innerY - 2);
    M5Cardputer.Display.print("^");
  }
  if (viewScroll < maxScroll) {
    M5Cardputer.Display.setCursor(arrowX, innerY + (visible - 1) * LINE_H_2X);
    M5Cardputer.Display.print("v");
  }

  // Footer (fixed, always visible)
  int footerY = DICE_BOX_Y + DICE_BOX_H + 2;
  M5Cardputer.Display.setCursor(0, footerY);
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.println("S = back");
  M5Cardputer.Display.println("Enter = reroll");
  M5Cardputer.Display.println("Space = menu");
}

// ---------------- PIXEL-PERFECT WRAP ----------------
void computeViewLinesFromRolls() {
  viewLineCount = 0;
  viewScroll = 0;

  M5Cardputer.Display.setTextSize(2);
  const int innerW = DICE_BOX_W - 2 * DICE_BOX_PAD_X;

  if (resultRolls.length() == 0) {
    viewLines[viewLineCount++] = "(no rolls)";
    return;
  }

  String line = "";
  int start = 0;

  while (start < (int)resultRolls.length() && viewLineCount < 260) {
    int comma = resultRolls.indexOf(',', start);
    String token;
    bool hasComma = false;

    if (comma == -1) {
      token = resultRolls.substring(start);
      start = resultRolls.length();
    } else {
      token = resultRolls.substring(start, comma);
      start = comma + 1;
      hasComma = true;
    }

    token.trim();

    String piece = token;
    if (hasComma) piece += ", ";

    if (line.length() == 0) {
      line = piece;
      continue;
    }

    String candidate = line + piece;

    if ((int)M5Cardputer.Display.textWidth(candidate.c_str()) <= innerW) {
      line = candidate;
    } else {
      viewLines[viewLineCount++] = line;
      line = piece;
    }
  }

  if (line.length() > 0 && viewLineCount < 260) {
    while (line.length() > 0 && line[line.length() - 1] == ' ') line.remove(line.length() - 1);
    viewLines[viewLineCount++] = line;
  }

  if (viewLineCount == 0) {
    viewLines[viewLineCount++] = "(no rolls)";
  }
}

// ---------------- LOGIC ----------------
void rollDice() {
  if (selectedDieIndex < 0 || selectedDieIndex > 6) {
    resetToMenu();
    return;
  }

  int sides = diceTypes[selectedDieIndex];
  int total = 0;
  String rolls = "";

  for (int i = 0; i < numDice; i++) {
    int r = random(1, sides + 1);
    total += r;
    rolls += String(r);
    if (i < numDice - 1) rolls += ", ";
  }

  if (numDice == 1) {
    resultHeader = "D" + String(sides) + " = " + String(total);
  } else {
    resultHeader = String(numDice) + "D" + String(sides) + " = " + String(total);
  }

  resultRolls = rolls;
  computeViewLinesFromRolls();

  state = RESULT;
  drawResult();
}

void resetToMenu() {
  input = "";
  selectedDieIndex = -1;
  numDice = 1;

  resultHeader = "";
  resultRolls = "";
  viewLineCount = 0;
  viewScroll = 0;

  state = MENU;
  drawMenu();
}
