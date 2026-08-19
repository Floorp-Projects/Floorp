// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { KeyboardShortcutController } from "../controller.ts";
import {
  isBarePrintableKeyEvent,
  isChromeEditableFocused,
  type KeyboardShortcutFocusStoreReader,
} from "../editable-focus.ts";
import {
  KEYBOARD_SHORTCUT_CONFIG_PREF,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  setConfig,
  setEnabled,
} from "../config.ts";
import type { KeyboardShortcutConfig, Modifiers } from "../type.ts";

type FocusElement = Element & { isContentEditable?: boolean };

function withPrefs(fn: () => void): void {
  const hadEnabled = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_ENABLED_PREF,
  );
  const hadConfig = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
  );
  const savedEnabled = hadEnabled
    ? Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF)
    : null;
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF)
    : null;

  try {
    fn();
  } finally {
    if (hadEnabled && savedEnabled !== null) {
      Services.prefs.setBoolPref(
        KEYBOARD_SHORTCUT_ENABLED_PREF,
        savedEnabled,
      );
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    }
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(
        KEYBOARD_SHORTCUT_CONFIG_PREF,
        savedConfig,
      );
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_CONFIG_PREF);
    }
  }
}

function applyShortcut(key: string, modifiers: Modifiers): void {
  const config: KeyboardShortcutConfig = {
    enabled: true,
    shortcuts: {
      "typing-guard-test": {
        key,
        modifiers,
        action: "typing-guard-test",
      },
    },
  };
  setEnabled(true);
  setConfig(config);
}

function createFocusElement(
  localName: string,
  options: { contentEditable?: boolean; chromeHost?: boolean } = {},
): FocusElement {
  const element = {
    localName,
    isContentEditable: options.contentEditable ?? false,
    closest: () => options.chromeHost ? element : null,
  };
  return element as unknown as FocusElement;
}

function createFakeWindow(activeElement: FocusElement | null): Window {
  const target = new EventTarget();
  Object.defineProperty(target, "document", {
    configurable: true,
    value: { activeElement },
  });
  return target as unknown as Window;
}

function dispatchKey(
  win: Window,
  init: KeyboardEventInit & { key: string; code: string },
): KeyboardEvent {
  const event = new KeyboardEvent("keydown", {
    bubbles: true,
    cancelable: true,
    ...init,
  });
  win.dispatchEvent(event);
  return event;
}

function modifiers(overrides: Partial<Modifiers> = {}): Modifiers {
  return {
    alt: false,
    ctrl: false,
    meta: false,
    shift: false,
    ...overrides,
  };
}

function predicateEvent(
  overrides: Partial<KeyboardEvent> = {},
): KeyboardEvent {
  return {
    key: "z",
    altKey: false,
    ctrlKey: false,
    metaKey: false,
    shiftKey: false,
    isComposing: false,
    getModifierState: () => false,
    ...overrides,
  } as KeyboardEvent;
}

function testBarePrintablePredicateIsExact(): void {
  assertEquals(
    isBarePrintableKeyEvent(predicateEvent()),
    true,
    "a single unmodified printable key should be guarded",
  );
  for (
    const event of [
      predicateEvent({ key: "Escape" }),
      predicateEvent({ altKey: true }),
      predicateEvent({ ctrlKey: true }),
      predicateEvent({ metaKey: true }),
      predicateEvent({ shiftKey: true }),
      predicateEvent({ isComposing: true }),
      predicateEvent({ getModifierState: (name) => name === "AltGraph" }),
    ]
  ) {
    assertEquals(
      isBarePrintableKeyEvent(event),
      false,
      "non-bare, non-printable, composing, and AltGraph events stay active",
    );
  }
}

function testChromeEditableCoverage(): void {
  const cases: Array<[string, FocusElement]> = [
    ["input", createFocusElement("input")],
    ["textarea", createFocusElement("textarea")],
    ["contenteditable", createFocusElement("div", { contentEditable: true })],
    ["urlbar", createFocusElement("div", { chromeHost: true })],
    ["searchbar", createFocusElement("searchbar", { chromeHost: true })],
    ["findbar", createFocusElement("findbar", { chromeHost: true })],
  ];
  for (const [name, element] of cases) {
    assertEquals(
      isChromeEditableFocused(createFakeWindow(element)),
      true,
      `${name} focus should be recognized as chrome editable`,
    );
  }
  assertEquals(
    isChromeEditableFocused(createFakeWindow(createFocusElement("button"))),
    false,
    "a non-editable chrome control should not be guarded",
  );
}

function testChromeInputSuppressesBarePrintableShortcut(): void {
  withPrefs(() => {
    applyShortcut("Z", modifiers());
    const win = createFakeWindow(createFocusElement("input"));
    const controller = new KeyboardShortcutController(win);
    const event = dispatchKey(win, { key: "z", code: "KeyZ" });
    assertEquals(
      event.defaultPrevented,
      false,
      "typing in a chrome input should receive the bare printable key",
    );
    controller.destroy();
  });
}

function testRemoteInputSuppressesBarePrintableShortcut(): void {
  withPrefs(() => {
    applyShortcut("Z", modifiers());
    const browser = createFocusElement("browser");
    const win = createFakeWindow(browser);
    const store: KeyboardShortcutFocusStoreReader = {
      isEditableFocused(candidate): boolean {
        return candidate === browser;
      },
    };
    const controller = new KeyboardShortcutController(win, store);
    const event = dispatchKey(win, { key: "z", code: "KeyZ" });
    assertEquals(
      event.defaultPrevented,
      false,
      "remote editable state should suppress a bare printable shortcut",
    );
    controller.destroy();
  });
}

function testMissingRemoteStatePreservesBehavior(): void {
  withPrefs(() => {
    applyShortcut("Z", modifiers());
    const win = createFakeWindow(createFocusElement("browser"));
    const controller = new KeyboardShortcutController(win, null);
    const event = dispatchKey(win, { key: "z", code: "KeyZ" });
    assertEquals(
      event.defaultPrevented,
      true,
      "missing actor state should preserve existing shortcut dispatch",
    );
    controller.destroy();
  });
}

function testFailedRemoteStateReadPreservesBehavior(): void {
  withPrefs(() => {
    applyShortcut("Z", modifiers());
    const win = createFakeWindow(createFocusElement("browser"));
    const store: KeyboardShortcutFocusStoreReader = {
      isEditableFocused(): boolean {
        throw new Error("actor store unavailable");
      },
    };
    const controller = new KeyboardShortcutController(win, store);
    const event = dispatchKey(win, { key: "z", code: "KeyZ" });
    assertEquals(
      event.defaultPrevented,
      true,
      "a failed remote state read should preserve existing shortcut dispatch",
    );
    controller.destroy();
  });
}

function testChordRemainsActiveWhileRemoteTyping(): void {
  withPrefs(() => {
    applyShortcut("Z", modifiers({ ctrl: true }));
    const browser = createFocusElement("browser");
    const win = createFakeWindow(browser);
    const store: KeyboardShortcutFocusStoreReader = {
      isEditableFocused: () => true,
    };
    const controller = new KeyboardShortcutController(win, store);
    const event = dispatchKey(win, {
      key: "z",
      code: "KeyZ",
      ctrlKey: true,
    });
    assertEquals(
      event.defaultPrevented,
      true,
      "modified chords should remain active while remote content is editable",
    );
    controller.destroy();
  });
}

function testNonPrintableCommandRemainsActiveWhileRemoteTyping(): void {
  withPrefs(() => {
    applyShortcut("F2", modifiers());
    const browser = createFocusElement("browser");
    const win = createFakeWindow(browser);
    const store: KeyboardShortcutFocusStoreReader = {
      isEditableFocused: () => true,
    };
    const controller = new KeyboardShortcutController(win, store);
    const event = dispatchKey(win, { key: "F2", code: "F2" });
    assertEquals(
      event.defaultPrevented,
      true,
      "function-key commands should remain active while typing",
    );
    controller.destroy();
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "bare printable predicate is exact",
      fn: testBarePrintablePredicateIsExact,
    },
    { name: "chrome editable coverage", fn: testChromeEditableCoverage },
    {
      name: "chrome input suppresses bare printable shortcut",
      fn: testChromeInputSuppressesBarePrintableShortcut,
    },
    {
      name: "remote input suppresses bare printable shortcut",
      fn: testRemoteInputSuppressesBarePrintableShortcut,
    },
    {
      name: "missing remote state preserves behavior",
      fn: testMissingRemoteStatePreservesBehavior,
    },
    {
      name: "failed remote state read preserves behavior",
      fn: testFailedRemoteStateReadPreservesBehavior,
    },
    {
      name: "modified chord remains active while typing",
      fn: testChordRemainsActiveWhileRemoteTyping,
    },
    {
      name: "non-printable command remains active while typing",
      fn: testNonPrintableCommandRemainsActiveWhileRemoteTyping,
    },
  ];
  await runTests("keyboardShortcutTypingGuard.test.ts", tests);
}
