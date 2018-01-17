/**
 * Bug 1222285 - A test case for testing whether keyboard events be spoofed correctly
 *   when fingerprinting resistance is enable.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const nsIDOMKeyEvent = Ci.nsIDOMKeyEvent;
const SHOULD_DELIVER_KEYDOWN          = 0x1;
const SHOULD_DELIVER_KEYPRESS         = 0x2;
const SHOULD_DELIVER_KEYUP            = 0x4;
const SHOULD_DELIVER_ALL              = SHOULD_DELIVER_KEYDOWN |
                                        SHOULD_DELIVER_KEYPRESS |
                                        SHOULD_DELIVER_KEYUP;

const TEST_PATH = "http://example.net/browser/browser/" +
                  "components/resistfingerprinting/test/browser/";

// The test cases for english content.
const TEST_CASES_EN = [
  { key: "KEY_ArrowDown", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "ArrowDown", code: "ArrowDown", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_DOWN,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_ArrowLeft", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "ArrowLeft", code: "ArrowLeft", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_LEFT,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_ArrowRight", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "ArrowRight", code: "ArrowRight", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_RIGHT,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_ArrowUp", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "ArrowUp", code: "ArrowUp", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_UP,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_CapsLock", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_KEYDOWN,
    result: { key: "CapsLock", code: "CapsLock", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_CAPS_LOCK,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_End", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "End", code: "End", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_END,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_Enter", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Enter", code: "Enter", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_RETURN,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_Escape", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Escape", code: "Escape", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_ESCAPE,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_Home", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Home", code: "Home", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_HOME,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_Meta", modifiers: { location: KeyboardEvent.DOM_KEY_LOCATION_LEFT, metaKey: true },
    expectedKeyEvent: SHOULD_DELIVER_KEYDOWN,
    result: { key: "Meta", code: "OSLeft", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_WIN,
              location: KeyboardEvent.DOM_KEY_LOCATION_LEFT, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_Meta", modifiers: { location: KeyboardEvent.DOM_KEY_LOCATION_RIGHT, metaKey: true },
    expectedKeyEvent: SHOULD_DELIVER_KEYDOWN,
    result: { key: "Meta", code: "OSRight", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_WIN,
              location: KeyboardEvent.DOM_KEY_LOCATION_RIGHT, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_OS", modifiers: { location: KeyboardEvent.DOM_KEY_LOCATION_LEFT, osKey: true },
    expectedKeyEvent: SHOULD_DELIVER_KEYDOWN,
    result: { key: "OS", code: "OSLeft", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_WIN,
              location: KeyboardEvent.DOM_KEY_LOCATION_LEFT, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_OS", modifiers: { location: KeyboardEvent.DOM_KEY_LOCATION_RIGHT, osKey: true },
    expectedKeyEvent: SHOULD_DELIVER_KEYDOWN,
    result: { key: "OS", code: "OSRight", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_WIN,
              location: KeyboardEvent.DOM_KEY_LOCATION_RIGHT, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_PageDown", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "PageDown", code: "PageDown", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_PAGE_DOWN,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_PageUp", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "PageUp", code: "PageUp", charCode: 0, keyCode: nsIDOMKeyEvent.DOM_VK_PAGE_UP,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: " ", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: " ", code: "Space", charCode: 32, keyCode: nsIDOMKeyEvent.DOM_VK_SPACE,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ",", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ",", code: "Comma", charCode: 44, keyCode: nsIDOMKeyEvent.DOM_VK_COMMA,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "<", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "<", code: "Comma", charCode: 60, keyCode: nsIDOMKeyEvent.DOM_VK_COMMA,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "[", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "[", code: "BracketLeft", charCode: 91, keyCode: nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "{", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "{", code: "BracketLeft", charCode: 123, keyCode: nsIDOMKeyEvent.DOM_VK_OPEN_BRACKET,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "]", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "]", code: "BracketRight", charCode: 93, keyCode: nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "}", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "}", code: "BracketRight", charCode: 125, keyCode: nsIDOMKeyEvent.DOM_VK_CLOSE_BRACKET,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "\\", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "\\", code: "Backslash", charCode: 92, keyCode: nsIDOMKeyEvent.DOM_VK_BACK_SLASH,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "|", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "|", code: "Backslash", charCode: 124, keyCode: nsIDOMKeyEvent.DOM_VK_BACK_SLASH,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ";", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ";", code: "Semicolon", charCode: 59, keyCode: nsIDOMKeyEvent.DOM_VK_SEMICOLON,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ":", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ":", code: "Semicolon", charCode: 58, keyCode: nsIDOMKeyEvent.DOM_VK_SEMICOLON,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ".", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ".", code: "Period", charCode: 46, keyCode: nsIDOMKeyEvent.DOM_VK_PERIOD,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ">", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ">", code: "Period", charCode: 62, keyCode: nsIDOMKeyEvent.DOM_VK_PERIOD,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "/", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "/", code: "Slash", charCode: 47, keyCode: nsIDOMKeyEvent.DOM_VK_SLASH,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "?", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "?", code: "Slash", charCode: 63, keyCode: nsIDOMKeyEvent.DOM_VK_SLASH,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "'", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "'", code: "Quote", charCode: 39, keyCode: nsIDOMKeyEvent.DOM_VK_QUOTE,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "\"", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "\"", code: "Quote", charCode: 34, keyCode: nsIDOMKeyEvent.DOM_VK_QUOTE,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "-", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "-", code: "Minus", charCode: 45, keyCode: nsIDOMKeyEvent.DOM_VK_HYPHEN_MINUS,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "_", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "_", code: "Minus", charCode: 95, keyCode: nsIDOMKeyEvent.DOM_VK_HYPHEN_MINUS,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "=", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "=", code: "Equal", charCode: 61, keyCode: nsIDOMKeyEvent.DOM_VK_EQUALS,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "+", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "+", code: "Equal", charCode: 43, keyCode: nsIDOMKeyEvent.DOM_VK_EQUALS,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "a", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "a", code: "KeyA", charCode: 97, keyCode: nsIDOMKeyEvent.DOM_VK_A,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "A", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "A", code: "KeyA", charCode: 65, keyCode: nsIDOMKeyEvent.DOM_VK_A,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "b", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "b", code: "KeyB", charCode: 98, keyCode: nsIDOMKeyEvent.DOM_VK_B,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "B", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "B", code: "KeyB", charCode: 66, keyCode: nsIDOMKeyEvent.DOM_VK_B,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "c", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "c", code: "KeyC", charCode: 99, keyCode: nsIDOMKeyEvent.DOM_VK_C,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "C", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "C", code: "KeyC", charCode: 67, keyCode: nsIDOMKeyEvent.DOM_VK_C,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "d", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "d", code: "KeyD", charCode: 100, keyCode: nsIDOMKeyEvent.DOM_VK_D,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "D", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "D", code: "KeyD", charCode: 68, keyCode: nsIDOMKeyEvent.DOM_VK_D,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "e", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "e", code: "KeyE", charCode: 101, keyCode: nsIDOMKeyEvent.DOM_VK_E,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "E", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "E", code: "KeyE", charCode: 69, keyCode: nsIDOMKeyEvent.DOM_VK_E,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "f", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "f", code: "KeyF", charCode: 102, keyCode: nsIDOMKeyEvent.DOM_VK_F,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "F", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F", code: "KeyF", charCode: 70, keyCode: nsIDOMKeyEvent.DOM_VK_F,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "g", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "g", code: "KeyG", charCode: 103, keyCode: nsIDOMKeyEvent.DOM_VK_G,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "G", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "G", code: "KeyG", charCode: 71, keyCode: nsIDOMKeyEvent.DOM_VK_G,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "h", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "h", code: "KeyH", charCode: 104, keyCode: nsIDOMKeyEvent.DOM_VK_H,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "H", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "H", code: "KeyH", charCode: 72, keyCode: nsIDOMKeyEvent.DOM_VK_H,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "i", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "i", code: "KeyI", charCode: 105, keyCode: nsIDOMKeyEvent.DOM_VK_I,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "I", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "I", code: "KeyI", charCode: 73, keyCode: nsIDOMKeyEvent.DOM_VK_I,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "j", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "j", code: "KeyJ", charCode: 106, keyCode: nsIDOMKeyEvent.DOM_VK_J,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "J", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "J", code: "KeyJ", charCode: 74, keyCode: nsIDOMKeyEvent.DOM_VK_J,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "k", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "k", code: "KeyK", charCode: 107, keyCode: nsIDOMKeyEvent.DOM_VK_K,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "K", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "K", code: "KeyK", charCode: 75, keyCode: nsIDOMKeyEvent.DOM_VK_K,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "l", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "l", code: "KeyL", charCode: 108, keyCode: nsIDOMKeyEvent.DOM_VK_L,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "L", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "L", code: "KeyL", charCode: 76, keyCode: nsIDOMKeyEvent.DOM_VK_L,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "m", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "m", code: "KeyM", charCode: 109, keyCode: nsIDOMKeyEvent.DOM_VK_M,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "M", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "M", code: "KeyM", charCode: 77, keyCode: nsIDOMKeyEvent.DOM_VK_M,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "n", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "n", code: "KeyN", charCode: 110, keyCode: nsIDOMKeyEvent.DOM_VK_N,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "N", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "N", code: "KeyN", charCode: 78, keyCode: nsIDOMKeyEvent.DOM_VK_N,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "o", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "o", code: "KeyO", charCode: 111, keyCode: nsIDOMKeyEvent.DOM_VK_O,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "O", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "O", code: "KeyO", charCode: 79, keyCode: nsIDOMKeyEvent.DOM_VK_O,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "p", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "p", code: "KeyP", charCode: 112, keyCode: nsIDOMKeyEvent.DOM_VK_P,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "P", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "P", code: "KeyP", charCode: 80, keyCode: nsIDOMKeyEvent.DOM_VK_P,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "q", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "q", code: "KeyQ", charCode: 113, keyCode: nsIDOMKeyEvent.DOM_VK_Q,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "Q", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Q", code: "KeyQ", charCode: 81, keyCode: nsIDOMKeyEvent.DOM_VK_Q,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "r", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "r", code: "KeyR", charCode: 114, keyCode: nsIDOMKeyEvent.DOM_VK_R,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "R", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "R", code: "KeyR", charCode: 82, keyCode: nsIDOMKeyEvent.DOM_VK_R,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "s", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "s", code: "KeyS", charCode: 115, keyCode: nsIDOMKeyEvent.DOM_VK_S,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "S", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "S", code: "KeyS", charCode: 83, keyCode: nsIDOMKeyEvent.DOM_VK_S,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "t", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "t", code: "KeyT", charCode: 116, keyCode: nsIDOMKeyEvent.DOM_VK_T,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "T", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "T", code: "KeyT", charCode: 84, keyCode: nsIDOMKeyEvent.DOM_VK_T,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "u", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "u", code: "KeyU", charCode: 117, keyCode: nsIDOMKeyEvent.DOM_VK_U,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "U", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "U", code: "KeyU", charCode: 85, keyCode: nsIDOMKeyEvent.DOM_VK_U,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "v", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "v", code: "KeyV", charCode: 118, keyCode: nsIDOMKeyEvent.DOM_VK_V,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "V", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "V", code: "KeyV", charCode: 86, keyCode: nsIDOMKeyEvent.DOM_VK_V,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "w", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "w", code: "KeyW", charCode: 119, keyCode: nsIDOMKeyEvent.DOM_VK_W,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "W", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "W", code: "KeyW", charCode: 87, keyCode: nsIDOMKeyEvent.DOM_VK_W,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "x", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "x", code: "KeyX", charCode: 120, keyCode: nsIDOMKeyEvent.DOM_VK_X,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "X", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "X", code: "KeyX", charCode: 88, keyCode: nsIDOMKeyEvent.DOM_VK_X,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "y", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "y", code: "KeyY", charCode: 121, keyCode: nsIDOMKeyEvent.DOM_VK_Y,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "Y", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Y", code: "KeyY", charCode: 89, keyCode: nsIDOMKeyEvent.DOM_VK_Y,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "z", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "z", code: "KeyZ", charCode: 122, keyCode: nsIDOMKeyEvent.DOM_VK_Z,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "Z", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "Z", code: "KeyZ", charCode: 90, keyCode: nsIDOMKeyEvent.DOM_VK_Z,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "0", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "0", code: "Digit0", charCode: 48, keyCode: nsIDOMKeyEvent.DOM_VK_0,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "1", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "1", code: "Digit1", charCode: 49, keyCode: nsIDOMKeyEvent.DOM_VK_1,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "2", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "2", code: "Digit2", charCode: 50, keyCode: nsIDOMKeyEvent.DOM_VK_2,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "3", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "3", code: "Digit3", charCode: 51, keyCode: nsIDOMKeyEvent.DOM_VK_3,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "4", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "4", code: "Digit4", charCode: 52, keyCode: nsIDOMKeyEvent.DOM_VK_4,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "5", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "5", code: "Digit5", charCode: 53, keyCode: nsIDOMKeyEvent.DOM_VK_5,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "6", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "6", code: "Digit6", charCode: 54, keyCode: nsIDOMKeyEvent.DOM_VK_6,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "7", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "7", code: "Digit7", charCode: 55, keyCode: nsIDOMKeyEvent.DOM_VK_7,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "8", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "8", code: "Digit8", charCode: 56, keyCode: nsIDOMKeyEvent.DOM_VK_8,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "9", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "9", code: "Digit9", charCode: 57, keyCode: nsIDOMKeyEvent.DOM_VK_9,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: ")", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: ")", code: "Digit0", charCode: 41, keyCode: nsIDOMKeyEvent.DOM_VK_0,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "!", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "!", code: "Digit1", charCode: 33, keyCode: nsIDOMKeyEvent.DOM_VK_1,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "@", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "@", code: "Digit2", charCode: 64, keyCode: nsIDOMKeyEvent.DOM_VK_2,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "#", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "#", code: "Digit3", charCode: 35, keyCode: nsIDOMKeyEvent.DOM_VK_3,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "$", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "$", code: "Digit4", charCode: 36, keyCode: nsIDOMKeyEvent.DOM_VK_4,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "%", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "%", code: "Digit5", charCode: 37, keyCode: nsIDOMKeyEvent.DOM_VK_5,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "^", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "^", code: "Digit6", charCode: 94, keyCode: nsIDOMKeyEvent.DOM_VK_6,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "&", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "&", code: "Digit7", charCode: 38, keyCode: nsIDOMKeyEvent.DOM_VK_7,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "*", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "*", code: "Digit8", charCode: 42, keyCode: nsIDOMKeyEvent.DOM_VK_8,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "(", modifiers: { shiftKey: true }, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "(", code: "Digit9", charCode: 40, keyCode: nsIDOMKeyEvent.DOM_VK_9,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: true,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F1", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F1", code: "F1", charCode: 112, keyCode: nsIDOMKeyEvent.DOM_VK_F1,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F2", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F2", code: "F2", charCode: 113, keyCode: nsIDOMKeyEvent.DOM_VK_F2,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F3", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F3", code: "F3", charCode: 114, keyCode: nsIDOMKeyEvent.DOM_VK_F3,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F4", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F4", code: "F4", charCode: 115, keyCode: nsIDOMKeyEvent.DOM_VK_F4,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F5", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F5", code: "F5", charCode: 116, keyCode: nsIDOMKeyEvent.DOM_VK_F5,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F7", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F7", code: "F7", charCode: 118, keyCode: nsIDOMKeyEvent.DOM_VK_F7,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F8", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F8", code: "F8", charCode: 119, keyCode: nsIDOMKeyEvent.DOM_VK_F8,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F9", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F9", code: "F9", charCode: 120, keyCode: nsIDOMKeyEvent.DOM_VK_F9,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F10", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F10", code: "F10", charCode: 121, keyCode: nsIDOMKeyEvent.DOM_VK_F10,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F11", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F11", code: "F11", charCode: 122, keyCode: nsIDOMKeyEvent.DOM_VK_F11,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
  { key: "KEY_F12", modifiers: {}, expectedKeyEvent: SHOULD_DELIVER_ALL,
    result: { key: "F12", code: "F12", charCode: 123, keyCode: nsIDOMKeyEvent.DOM_VK_F12,
              location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
              ctrlKey: false, altGraphKey: false }
  },
];

async function testKeyEvent(aTab, aTestCase) {
  // Prepare all expected key events.
  let testEvents = [];

  if (aTestCase.expectedKeyEvent & SHOULD_DELIVER_KEYDOWN) {
    testEvents.push("keydown");
  }

  if (aTestCase.expectedKeyEvent & SHOULD_DELIVER_KEYPRESS) {
    testEvents.push("keypress");
  }

  if (aTestCase.expectedKeyEvent & SHOULD_DELIVER_KEYUP) {
    testEvents.push("keyup");
  }

  let allKeyEventPromises = [];

  for (let testEvent of testEvents) {
    let keyEventPromise = ContentTask.spawn(aTab.linkedBrowser, {testEvent, result: aTestCase.result}, async (aInput) => {
      function verifyKeyboardEvent(aEvent, aResult) {
        is(aEvent.key, aResult.key, "KeyboardEvent.key is correctly spoofed.");
        is(aEvent.code, aResult.code, "KeyboardEvent.code is correctly spoofed.");
        is(aEvent.location, aResult.location, "KeyboardEvent.location is correctly spoofed.");
        is(aEvent.altKey, aResult.altKey, "KeyboardEvent.altKey is correctly spoofed.");
        is(aEvent.shiftKey, aResult.shiftKey, "KeyboardEvent.shiftKey is correctly spoofed.");
        is(aEvent.ctrlKey, aResult.ctrlKey, "KeyboardEvent.ctrlKey is correctly spoofed.");

        // If the charCode is not 0, this is a character. The keyCode will be remained as 0.
        // Otherwise, we should check the keyCode.
        if (aEvent.charCode != 0) {
          is(aEvent.keyCode, 0, "KeyboardEvent.keyCode should be 0 for this case.");
          is(aEvent.charCode, aResult.charCode, "KeyboardEvent.charCode is correctly spoofed.");
        } else {
          is(aEvent.keyCode, aResult.keyCode, "KeyboardEvent.keyCode is correctly spoofed.");
          is(aEvent.charCode, 0, "KeyboardEvent.charCode should be 0 for this case.");
        }

        // Check getModifierState().
        is(aEvent.modifierState.Alt, aResult.altKey,
            "KeyboardEvent.getModifierState() reports a correctly spoofed value for 'Alt'.");
        is(aEvent.modifierState.AltGraph, aResult.altGraphKey,
            "KeyboardEvent.getModifierState() reports a correctly spoofed value for 'AltGraph'.");
        is(aEvent.modifierState.Shift, aResult.shiftKey,
            `KeyboardEvent.getModifierState() reports a correctly spoofed value for 'Shift'.`);
        is(aEvent.modifierState.Control, aResult.ctrlKey,
            `KeyboardEvent.getModifierState() reports a correctly spoofed value for 'Control'.`);
      }

      let {testEvent: eventType, result} = aInput;
      let inputBox = content.document.getElementById("test");

      // We need to put the real access of event object into the content page instead of
      // here, ContentTask.spawn, since the script running here is under chrome privilege.
      // So the fingerprinting resistance won't work here.
      let resElement = content.document.getElementById("result-" + eventType);

      // First, try to focus on the input box.
      await new Promise(resolve => {
        if (content.document.activeElement == inputBox) {
          // the input box already got focused.
          resolve();
        } else {
          inputBox.onfocus = () => {
            resolve();
          };
          inputBox.focus();
        }
      });

      // Once the result of the keyboard event ready, the content page will send
      // a custom event 'resultAvailable' for informing the script to check the
      // result.
      await new Promise(resolve => {
        function eventHandler(aEvent) {
          verifyKeyboardEvent(JSON.parse(resElement.value), result);
          resElement.removeEventListener("resultAvailable", eventHandler, true);
          resolve();
        }

        resElement.addEventListener("resultAvailable", eventHandler, true);
      });
    });

    allKeyEventPromises.push(keyEventPromise);
  }

  // Send key event to the tab.
  BrowserTestUtils.synthesizeKey(aTestCase.key, aTestCase.modifiers, aTab.linkedBrowser);

  await Promise.all(allKeyEventPromises);
}

function eventConsumer(aEvent) {
  aEvent.preventDefault();
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });
});

add_task(async function runTestsForEnglishContent() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_keyBoardEvent.sjs?language=en-US");

  // Prevent shortcut keys.
  gBrowser.addEventListener("keypress", eventConsumer, true);

  for (let test of TEST_CASES_EN) {
    await testKeyEvent(tab, test);
  }

  // Test a key which doesn't exist in US English keyboard layout.
  await testKeyEvent(tab,
    {
      key: "\u00DF", modifiers: { code: "Minus", keyCode: 63 }, expectedKeyEvent: SHOULD_DELIVER_ALL,
      result: { key: "\u00DF", code: "", charCode: 223, keyCode: 0,
                location: KeyboardEvent.DOM_KEY_LOCATION_STANDARD, altKey: false, shiftKey: false,
                ctrlKey: false, altGraphKey: false }
    }
  );

  gBrowser.removeEventListener("keypress", eventConsumer, true);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runTestForSuppressModifierKeys() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_keyBoardEvent.sjs?language=en-US");

  // Prevent Alt key to trigger the menu item.
  gBrowser.addEventListener("keydown", eventConsumer, true);

  for (let eventType of ["keydown", "keyup"]) {
    for (let modifierKey of ["Alt", "Shift", "Control"]) {
      let testPromise = ContentTask.spawn(tab.linkedBrowser, eventType, async (aEventType) => {
        let inputBox = content.document.getElementById("test");

        // First, try to focus on the input box.
        await new Promise(resolve => {
          if (content.document.activeElement == inputBox) {
            // the input box already got focused.
            resolve();
          } else {
            inputBox.onfocus = () => {
              resolve();
            };
            inputBox.focus();
          }
        });

        let event = await new Promise(resolve => {
          inputBox.addEventListener(aEventType, (aEvent) => {
            resolve(aEvent);
          }, {once: true});
        });

        is(event.key, "x", "'x' should be seen and the modifier key should be suppressed");
      });

      let modifierState;

      if (modifierKey === "Alt") {
        modifierState = {altKey: true};
      } else if (modifierKey === "Shift") {
        modifierState = {shiftKey: true};
      } else {
        modifierState = {ctrlKey: true};
      }

      // Generate a Alt or Shift key event.
      BrowserTestUtils.synthesizeKey(`KEY_${modifierKey}`, modifierState, tab.linkedBrowser);

      // Generate a dummy "x" key event that will only be handled if
      // modifier key is successfully suppressed.
      BrowserTestUtils.synthesizeKey("x", {}, tab.linkedBrowser);

      await testPromise;
    }
  }

  gBrowser.removeEventListener("keydown", eventConsumer, true);

  await BrowserTestUtils.removeTab(tab);
});

