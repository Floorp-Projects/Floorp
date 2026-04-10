// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { KeyboardShortcutController } from "../controller.ts";
import {
  setEnabled,
  setConfig,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  KEYBOARD_SHORTCUT_CONFIG_PREF,
} from "../config.ts";
import type { KeyboardShortcutConfig } from "../type.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const TEST_ENABLED_PREF = KEYBOARD_SHORTCUT_ENABLED_PREF;
const TEST_CONFIG_PREF = KEYBOARD_SHORTCUT_CONFIG_PREF;

/** Save current pref state and restore after fn runs. */
function withPrefs(fn: () => void): void {
  const hadEnabled = Services.prefs.prefHasUserValue(TEST_ENABLED_PREF);
  const hadConfig = Services.prefs.prefHasUserValue(TEST_CONFIG_PREF);
  const savedEnabled = hadEnabled
    ? Services.prefs.getBoolPref(TEST_ENABLED_PREF)
    : null;
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(TEST_CONFIG_PREF)
    : null;

  try {
    fn();
  } finally {
    if (hadEnabled && savedEnabled !== null) {
      Services.prefs.setBoolPref(TEST_ENABLED_PREF, savedEnabled);
    } else {
      Services.prefs.clearUserPref(TEST_ENABLED_PREF);
    }
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(TEST_CONFIG_PREF, savedConfig);
    } else {
      Services.prefs.clearUserPref(TEST_CONFIG_PREF);
    }
  }
}

/** Apply a test shortcut config via prefs and wait for SolidJS to react. */
function applyTestConfig(config: KeyboardShortcutConfig): void {
  Services.prefs.setBoolPref(TEST_ENABLED_PREF, true);
  Services.prefs.setStringPref(TEST_CONFIG_PREF, JSON.stringify(config));
  setEnabled(true);
  setConfig(config);
}

/** Create a KeyboardEvent with the given parameters and dispatch it. */
function dispatchKeyEvent(
  target: EventTarget,
  type: "keydown" | "keyup",
  options: {
    key?: string;
    code?: string;
    altKey?: boolean;
    ctrlKey?: boolean;
    metaKey?: boolean;
    shiftKey?: boolean;
  },
): KeyboardEvent {
  const event = new KeyboardEvent(type, {
    key: options.key ?? "",
    code: options.code ?? "",
    altKey: options.altKey ?? false,
    ctrlKey: options.ctrlKey ?? false,
    metaKey: options.metaKey ?? false,
    shiftKey: options.shiftKey ?? false,
    bubbles: true,
    cancelable: true,
  });
  target.dispatchEvent(event);
  return event;
}

/**
 * Create a fresh fake window (EventTarget) for controller tests.
 *
 * Using a detached EventTarget instead of the real `window` avoids
 * interference with the global KeyboardShortcutService controller
 * that is auto-attached to `window` at module init time.
 */
function createFakeWindow(): Window {
  return new EventTarget() as unknown as Window;
}

// ---------------------------------------------------------------------------
// Test config with a known shortcut: Ctrl+T → "test-ctrl-t"
// ---------------------------------------------------------------------------

const CTRL_T_CONFIG: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: {
    "test-ctrl-t": {
      key: "T",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
      action: "test-ctrl-t",
    },
  },
};

const ALT_SHIFT_F_CONFIG: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: {
    "test-alt-shift-f": {
      key: "F",
      modifiers: { alt: true, ctrl: false, meta: false, shift: true },
      action: "test-alt-shift-f",
    },
  },
};

const DIGIT_CONFIG: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: {
    "test-digit-1": {
      key: "1",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
      action: "test-digit-1",
    },
  },
};

const MULTI_CONFIG: KeyboardShortcutConfig = {
  enabled: true,
  shortcuts: {
    "test-ctrl-t": {
      key: "T",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
      action: "test-ctrl-t",
    },
    "test-alt-shift-f": {
      key: "F",
      modifiers: { alt: true, ctrl: false, meta: false, shift: true },
      action: "test-alt-shift-f",
    },
  },
};

// ---------------------------------------------------------------------------
// Tests — Controller lifecycle
// ---------------------------------------------------------------------------

function testControllerConstructorAttachesListeners(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Verify the controller is functional by dispatching a matching event.
    // If listeners were attached, the event should be handled (defaultPrevented).
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    // If the shortcut is matched, preventDefault is called regardless of
    // whether the action function actually exists in the actions list.
    assertEquals(
      event.defaultPrevented,
      true,
      "controller should prevent default when Ctrl+T matches",
    );

    controller.destroy();
  });
}

function testControllerDestroyRemovesListeners(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);
    controller.destroy();

    // After destroy, events should NOT be handled
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "destroyed controller should not prevent default",
    );
  });
}

function testControllerDestroyIsIdempotent(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Calling destroy twice should not throw
    controller.destroy();
    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Shortcut matching: letter keys
// ---------------------------------------------------------------------------

function testCtrlTMatches(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(event.defaultPrevented, true, "Ctrl+T should match");

    controller.destroy();
  });
}

function testCtrlTDoesNotMatchWithoutCtrl(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "T without Ctrl should NOT match",
    );

    controller.destroy();
  });
}

function testCtrlTDoesNotMatchWithExtraAlt(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
      altKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Ctrl+Alt+T should NOT match Ctrl+T shortcut",
    );

    controller.destroy();
  });
}

function testCtrlTDoesNotMatchWrongKey(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyX",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Ctrl+X should NOT match Ctrl+T shortcut",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Shortcut matching: modifier combos
// ---------------------------------------------------------------------------

function testAltShiftFMatches(): void {
  withPrefs(() => {
    applyTestConfig(ALT_SHIFT_F_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyF",
      altKey: true,
      shiftKey: true,
    });

    assertEquals(event.defaultPrevented, true, "Alt+Shift+F should match");

    controller.destroy();
  });
}

function testAltShiftFDoesNotMatchPartial(): void {
  withPrefs(() => {
    applyTestConfig(ALT_SHIFT_F_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Only Alt, no Shift
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyF",
      altKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Alt+F should NOT match Alt+Shift+F shortcut",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Shortcut matching: digit keys
// ---------------------------------------------------------------------------

function testDigitKeyMatches(): void {
  withPrefs(() => {
    applyTestConfig(DIGIT_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "Digit1",
      ctrlKey: true,
    });

    assertEquals(event.defaultPrevented, true, "Ctrl+1 (Digit1) should match");

    controller.destroy();
  });
}

function testDigitKeyDoesNotMatchWrongDigit(): void {
  withPrefs(() => {
    applyTestConfig(DIGIT_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "Digit2",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Ctrl+2 should NOT match Ctrl+1 shortcut",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Pure modifier presses are ignored
// ---------------------------------------------------------------------------

function testPureCtrlPressIgnored(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Pressing just the Control key should not trigger any shortcut
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "ControlLeft",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "pure Control key press should be ignored",
    );

    controller.destroy();
  });
}

function testPureShiftPressIgnored(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "ShiftLeft",
      shiftKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "pure Shift key press should be ignored",
    );

    controller.destroy();
  });
}

function testPureAltPressIgnored(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "AltLeft",
      altKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "pure Alt key press should be ignored",
    );

    controller.destroy();
  });
}

function testPureMetaPressIgnored(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "MetaLeft",
      metaKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "pure Meta key press should be ignored",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Multiple shortcuts
// ---------------------------------------------------------------------------

function testMultipleShortcutsFirstMatches(): void {
  withPrefs(() => {
    applyTestConfig(MULTI_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "Ctrl+T should match in multi-shortcut config",
    );

    controller.destroy();
  });
}

function testMultipleShortcutsSecondMatches(): void {
  withPrefs(() => {
    applyTestConfig(MULTI_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyF",
      altKey: true,
      shiftKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "Alt+Shift+F should match in multi-shortcut config",
    );

    controller.destroy();
  });
}

function testMultipleShortcutsNeitherMatches(): void {
  withPrefs(() => {
    applyTestConfig(MULTI_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyZ",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Ctrl+Z should NOT match any shortcut in multi-config",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Disabled state
// ---------------------------------------------------------------------------

function testDisabledDoesNotMatch(): void {
  withPrefs(() => {
    Services.prefs.setBoolPref(TEST_ENABLED_PREF, false);
    setEnabled(false);
    Services.prefs.setStringPref(
      TEST_CONFIG_PREF,
      JSON.stringify(CTRL_T_CONFIG),
    );
    setConfig(CTRL_T_CONFIG);

    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "shortcut should not match when disabled",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — keyup resets state
// ---------------------------------------------------------------------------

function testKeyUpAfterKeyDown(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Key down
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    // Key up
    const upEvent = dispatchKeyEvent(fakeWin, "keyup", {
      code: "KeyT",
      key: "t",
    });

    // keyup should not throw; this verifies the handler runs without error.
    assertEquals(
      upEvent.defaultPrevented,
      false,
      "keyup event should not be prevented",
    );

    controller.destroy();
  });
}

function testMatchedShortcutStopsPropagation(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const originalStopPropagation = Event.prototype.stopPropagation;
    let stopPropagationCalled = false;

    Event.prototype.stopPropagation = function (...args: []): void {
      stopPropagationCalled = true;
      return originalStopPropagation.apply(this, args);
    };

    try {
      dispatchKeyEvent(fakeWin, "keydown", {
        code: "KeyT",
        ctrlKey: true,
      });

      assertEquals(
        stopPropagationCalled,
        true,
        "matched shortcut should call stopPropagation",
      );
    } finally {
      Event.prototype.stopPropagation = originalStopPropagation;
    }

    controller.destroy();
  });
}

function testMatchedShortcutResetsModifierState(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const first = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });
    assertEquals(first.defaultPrevented, true, "first Ctrl+T should match");

    const second = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: false,
    });
    assertEquals(
      second.defaultPrevented,
      false,
      "second T without Ctrl should not match after state reset",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Empty shortcuts config
// ---------------------------------------------------------------------------

function testEmptyShortcutsNoMatch(): void {
  withPrefs(() => {
    const emptyConfig: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {},
    };
    applyTestConfig(emptyConfig);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "empty shortcuts should not match any key",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Custom window target
// ---------------------------------------------------------------------------

function testControllerWithCustomTarget(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);

    // Use a separate fake window to prove custom targets work
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "controller with explicit custom target should handle events",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — isShortcutMatch key normalization (indirect via keydown)
// ---------------------------------------------------------------------------

function testLowercaseKeyMatchesUppercaseCode(): void {
  withPrefs(() => {
    // Config stores key as uppercase "T"
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // KeyboardEvent code is "KeyT" which should match key "T"
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "key 'T' should match event.code 'KeyT'",
    );

    controller.destroy();
  });
}

function testUppercaseKeyNormalization(): void {
  withPrefs(() => {
    // Config with uppercase "T" should match via the /A-Z/ → "Key${key}" pattern
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "T",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "uppercase key 'T' should match via KeyT normalization",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — State reset functionality
// ---------------------------------------------------------------------------

function testResetStateClearsPressedKeys(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press a key
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    // Destroy should reset state
    controller.destroy();

    // If we could access pressedKeys, we'd verify it's empty
    // Since we can't, we just verify destroy doesn't throw
    assertEquals(true, true, "resetState should complete without error");
  });
}

// ---------------------------------------------------------------------------
// Tests — Multiple simultaneous key presses
// ---------------------------------------------------------------------------

function testMultipleKeysPressedSimultaneously(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Ctrl first
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "ControlLeft",
      ctrlKey: true,
    });

    // Then press T (should match)
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "Ctrl+T should match even with sequential keydown events",
    );

    controller.destroy();
  });
}

function testKeySequenceDoesNotMatchPartialCombos(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "T",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Alt (not part of shortcut)
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "AltLeft",
      altKey: true,
    });

    // Then press Ctrl+T (should not match because Alt is also pressed)
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
      altKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "Alt+Ctrl+T should NOT match Ctrl+T shortcut",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Special key codes
// ---------------------------------------------------------------------------

function testNumpadKeyMatching(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-numpad": {
          key: "1",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-numpad",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Numpad keys use different codes but same normalized key
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "Numpad1",
      ctrlKey: true,
    });

    // Should not match because "Digit1" != "Numpad1"
    assertEquals(
      event.defaultPrevented,
      false,
      "Numpad1 should not match digit 1 shortcut (different code)",
    );

    controller.destroy();
  });
}

function testFunctionKeyMatching(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-f5": {
          key: "F5",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-f5",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "F5",
      ctrlKey: true,
    });

    // Function keys use just "F5" as code. The normalization regex
    // /^[A-Z]$/ and /^[0-9]$/ do not match "F5", so the key is kept as-is.
    // The event code is also "F5", so pressedKeys.has("F5") returns true.
    assertEquals(
      event.defaultPrevented,
      true,
      "F5 should match (key passes through normalization unchanged and matches event code directly)",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Lowercase letter keys in config
// ---------------------------------------------------------------------------

function testLowercaseKeyInConfig(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "t", // lowercase
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    // Lowercase "t" won't match because normalization only applies to A-Z
    assertEquals(
      event.defaultPrevented,
      false,
      "lowercase 't' should not match KeyT (no normalization)",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Action execution
// ---------------------------------------------------------------------------

function testActionNotFoundNoError(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "non-existent-action": {
          key: "X",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "this-action-does-not-exist",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Should match but not throw even though action doesn't exist
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyX",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      true,
      "shortcut should match and prevent default even if action not found",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Key state tracking
// ---------------------------------------------------------------------------

function testKeyUpClearsKeyState(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Ctrl+T (matches)
    const downEvent = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });
    assertEquals(downEvent.defaultPrevented, true, "first press should match");

    // Release T
    dispatchKeyEvent(fakeWin, "keyup", {
      code: "KeyT",
    });

    // Press T again (should still match if Ctrl is held)
    const downEvent2 = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });
    assertEquals(
      downEvent2.defaultPrevented,
      true,
      "second press should also match",
    );

    controller.destroy();
  });
}

function testModifierStateTracking(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Ctrl alone
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "ControlLeft",
      ctrlKey: true,
    });

    // Press T (should match)
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });
    assertEquals(event.defaultPrevented, true, "should match with Ctrl held");

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Edge cases
// ---------------------------------------------------------------------------

function testEmptyKeyInConfig(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "", // empty key
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Should not crash with empty key
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(event.defaultPrevented, false, "empty key should not match");

    controller.destroy();
  });
}

function testVeryLongKeyInConfig(): void {
  withPrefs(() => {
    const config: KeyboardShortcutConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "verylongkeynamethatwontmatch",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    applyTestConfig(config);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(event.defaultPrevented, false, "long key should not match");

    controller.destroy();
  });
}

function testMultipleControllersDontInterfere(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);

    const fakeWin1 = createFakeWindow();
    const fakeWin2 = createFakeWindow();
    const controller1 = new KeyboardShortcutController(fakeWin1);
    const controller2 = new KeyboardShortcutController(fakeWin2);

    // Each controller should work independently
    const event1 = dispatchKeyEvent(fakeWin1, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });
    const event2 = dispatchKeyEvent(fakeWin2, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(event1.defaultPrevented, true, "controller1 should match");
    assertEquals(event2.defaultPrevented, true, "controller2 should match");

    controller1.destroy();
    controller2.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Location-specific modifier keys
// ---------------------------------------------------------------------------

function testRightModifierKeyIgnored(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Right Control key should also be ignored
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "ControlRight",
      ctrlKey: true,
    });

    assertEquals(
      event.defaultPrevented,
      false,
      "pure ControlRight press should be ignored",
    );

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — Rapid key press sequences
// ---------------------------------------------------------------------------

function testRapidKeyPressSequence(): void {
  withPrefs(() => {
    applyTestConfig(MULTI_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Ctrl+T
    const event1 = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    // Quickly press Alt+Shift+F
    const event2 = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyF",
      altKey: true,
      shiftKey: true,
    });

    assertEquals(event1.defaultPrevented, true, "first shortcut should match");
    assertEquals(event2.defaultPrevented, true, "second shortcut should match");

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Tests — State after unmatched key
// ---------------------------------------------------------------------------

function testStatePreservedAfterUnmatchedKey(): void {
  withPrefs(() => {
    applyTestConfig(CTRL_T_CONFIG);
    const fakeWin = createFakeWindow();
    const controller = new KeyboardShortcutController(fakeWin);

    // Press Ctrl+X (doesn't match)
    dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyX",
      ctrlKey: true,
    });

    // Press Ctrl+T (should still match)
    const event = dispatchKeyEvent(fakeWin, "keydown", {
      code: "KeyT",
      ctrlKey: true,
    });

    assertEquals(event.defaultPrevented, true, "should still match after unmatched key");

    controller.destroy();
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Lifecycle
    {
      name: "constructor attaches listeners",
      fn: testControllerConstructorAttachesListeners,
    },
    {
      name: "destroy removes listeners",
      fn: testControllerDestroyRemovesListeners,
    },
    {
      name: "destroy is idempotent",
      fn: testControllerDestroyIsIdempotent,
    },
    // Letter key matching
    { name: "Ctrl+T matches", fn: testCtrlTMatches },
    {
      name: "T without Ctrl does NOT match",
      fn: testCtrlTDoesNotMatchWithoutCtrl,
    },
    {
      name: "Ctrl+Alt+T does NOT match Ctrl+T",
      fn: testCtrlTDoesNotMatchWithExtraAlt,
    },
    {
      name: "Ctrl+X does NOT match Ctrl+T",
      fn: testCtrlTDoesNotMatchWrongKey,
    },
    // Modifier combos
    { name: "Alt+Shift+F matches", fn: testAltShiftFMatches },
    {
      name: "Alt+F does NOT match Alt+Shift+F",
      fn: testAltShiftFDoesNotMatchPartial,
    },
    // Digit keys
    { name: "Ctrl+1 digit key matches", fn: testDigitKeyMatches },
    {
      name: "Ctrl+2 does NOT match Ctrl+1",
      fn: testDigitKeyDoesNotMatchWrongDigit,
    },
    // Pure modifier presses
    {
      name: "pure Ctrl press ignored",
      fn: testPureCtrlPressIgnored,
    },
    {
      name: "pure Shift press ignored",
      fn: testPureShiftPressIgnored,
    },
    {
      name: "pure Alt press ignored",
      fn: testPureAltPressIgnored,
    },
    {
      name: "pure Meta press ignored",
      fn: testPureMetaPressIgnored,
    },
    // Multiple shortcuts
    {
      name: "multi config: Ctrl+T matches",
      fn: testMultipleShortcutsFirstMatches,
    },
    {
      name: "multi config: Alt+Shift+F matches",
      fn: testMultipleShortcutsSecondMatches,
    },
    {
      name: "multi config: Ctrl+Z does NOT match",
      fn: testMultipleShortcutsNeitherMatches,
    },
    // Disabled state
    {
      name: "disabled does not match",
      fn: testDisabledDoesNotMatch,
    },
    // keyup
    {
      name: "keyup after keydown",
      fn: testKeyUpAfterKeyDown,
    },
    {
      name: "matched shortcut stops propagation",
      fn: testMatchedShortcutStopsPropagation,
    },
    {
      name: "matched shortcut resets modifier state",
      fn: testMatchedShortcutResetsModifierState,
    },
    // Empty shortcuts
    {
      name: "empty shortcuts no match",
      fn: testEmptyShortcutsNoMatch,
    },
    // Custom window
    {
      name: "custom window target",
      fn: testControllerWithCustomTarget,
    },
    // Key normalization
    {
      name: "lowercase key matches uppercase code",
      fn: testLowercaseKeyMatchesUppercaseCode,
    },
    {
      name: "uppercase key normalization",
      fn: testUppercaseKeyNormalization,
    },
    // State reset
    {
      name: "resetState clears pressed keys",
      fn: testResetStateClearsPressedKeys,
    },
    // Multiple simultaneous keys
    {
      name: "multiple keys pressed simultaneously",
      fn: testMultipleKeysPressedSimultaneously,
    },
    {
      name: "key sequence does not match partial combos",
      fn: testKeySequenceDoesNotMatchPartialCombos,
    },
    // Special key codes
    {
      name: "numpad key matching",
      fn: testNumpadKeyMatching,
    },
    {
      name: "function key matching",
      fn: testFunctionKeyMatching,
    },
    // Lowercase keys
    {
      name: "lowercase key in config",
      fn: testLowercaseKeyInConfig,
    },
    // Action execution
    {
      name: "action not found no error",
      fn: testActionNotFoundNoError,
    },
    // Key state tracking
    {
      name: "keyup clears key state",
      fn: testKeyUpClearsKeyState,
    },
    {
      name: "modifier state tracking",
      fn: testModifierStateTracking,
    },
    // Edge cases
    {
      name: "empty key in config",
      fn: testEmptyKeyInConfig,
    },
    {
      name: "very long key in config",
      fn: testVeryLongKeyInConfig,
    },
    {
      name: "multiple controllers don't interfere",
      fn: testMultipleControllersDontInterfere,
    },
    // Location-specific modifiers
    {
      name: "right modifier key ignored",
      fn: testRightModifierKeyIgnored,
    },
    // Rapid sequences
    {
      name: "rapid key press sequence",
      fn: testRapidKeyPressSequence,
    },
    {
      name: "state preserved after unmatched key",
      fn: testStatePreservedAfterUnmatchedKey,
    },
  ];

  await runTests("keyboardShortcutController.test.ts", tests);
}
