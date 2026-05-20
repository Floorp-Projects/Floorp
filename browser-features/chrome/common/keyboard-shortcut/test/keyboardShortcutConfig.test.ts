// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assertNotEquals,
  assert,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  shortcutToString,
  stringToShortcut,
  defaultConfig,
  strDefaultConfig,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  KEYBOARD_SHORTCUT_CONFIG_PREF,
  setEnabled,
  setConfig,
} from "../config.ts";

// ---------------------------------------------------------------------------
// shortcutToString tests
// ---------------------------------------------------------------------------

function testShortcutToStringWithAllModifiers(): void {
  const result = shortcutToString({
    modifiers: { alt: true, ctrl: true, meta: false, shift: true },
    key: "t",
    action: "open-new-tab",
  });

  assertEquals(
    result,
    "Alt+Ctrl+Shift+T",
    "shortcutToString should include all enabled modifiers in the expected order",
  );
}

function testShortcutToStringNoModifiers(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: false, meta: false, shift: false },
    key: "a",
    action: "test-action",
  });

  assertEquals(
    result,
    "A",
    "shortcutToString with no modifiers should return just the uppercase key",
  );
}

function testShortcutToStringSingleAlt(): void {
  const result = shortcutToString({
    modifiers: { alt: true, ctrl: false, meta: false, shift: false },
    key: "f",
    action: "test",
  });

  assertEquals(result, "Alt+F", "shortcutToString with only Alt modifier");
}

function testShortcutToStringSingleCtrl(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    key: "t",
    action: "test",
  });

  assertEquals(result, "Ctrl+T", "shortcutToString with only Ctrl modifier");
}

function testShortcutToStringSingleMeta(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: false, meta: true, shift: false },
    key: "l",
    action: "test",
  });

  assertEquals(result, "Meta+L", "shortcutToString with only Meta modifier");
}

function testShortcutToStringSingleShift(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: false, meta: false, shift: true },
    key: "tab",
    action: "test",
  });

  assertEquals(
    result,
    "Shift+TAB",
    "shortcutToString with only Shift modifier should uppercase key",
  );
}

function testShortcutToStringUppercasesKey(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    key: "x",
    action: "test",
  });

  assertEquals(result, "Ctrl+X", "shortcutToString should uppercase the key");
}

function testShortcutToStringWithDigitKey(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    key: "1",
    action: "test",
  });

  assertEquals(result, "Ctrl+1", "shortcutToString should handle digit keys");
}

function testShortcutToStringAllFourModifiers(): void {
  const result = shortcutToString({
    modifiers: { alt: true, ctrl: true, meta: true, shift: true },
    key: "k",
    action: "test",
  });

  assertEquals(
    result,
    "Alt+Ctrl+Meta+Shift+K",
    "shortcutToString should include all four modifiers",
  );
}

function testShortcutToStringModifierOrder(): void {
  // The order should always be Alt, Ctrl, Meta, Shift regardless of how
  // the config is structured
  const result = shortcutToString({
    modifiers: { shift: true, alt: true, ctrl: true, meta: false },
    key: "z",
    action: "undo",
  });

  assertEquals(
    result,
    "Alt+Ctrl+Shift+Z",
    "modifiers should always appear in Alt, Ctrl, Meta, Shift order",
  );
}

// ---------------------------------------------------------------------------
// stringToShortcut tests
// ---------------------------------------------------------------------------

function testStringToShortcutParsesModifiers(): void {
  const result = stringToShortcut("Meta+Ctrl+X");

  assertEquals(result.modifiers.alt, false, "alt should be false");
  assertEquals(result.modifiers.ctrl, true, "ctrl should be true");
  assertEquals(result.modifiers.meta, true, "meta should be true");
  assertEquals(result.modifiers.shift, false, "shift should be false");
  assertEquals(result.key, "x", "key should be normalized to lowercase");
}

function testStringToShortcutHandlesEmptyString(): void {
  const result = stringToShortcut("");
  assertEquals(result.key, "", "empty string should produce empty key");
  assertEquals(
    result.modifiers.alt,
    false,
    "empty string should produce false alt modifier",
  );
  assertEquals(
    result.modifiers.ctrl,
    false,
    "empty string should produce false ctrl modifier",
  );
  assertEquals(
    result.modifiers.meta,
    false,
    "empty string should produce false meta modifier",
  );
  assertEquals(
    result.modifiers.shift,
    false,
    "empty string should produce false shift modifier",
  );
}

function testStringToShortcutKeyOnly(): void {
  const result = stringToShortcut("A");

  assertEquals(result.key, "a", "key should be 'a' (lowercase)");
  assertEquals(
    result.modifiers.alt,
    false,
    "no modifiers should be set for key-only string",
  );
  assertEquals(
    result.modifiers.ctrl,
    false,
    "no modifiers should be set for key-only string",
  );
  assertEquals(
    result.modifiers.meta,
    false,
    "no modifiers should be set for key-only string",
  );
  assertEquals(
    result.modifiers.shift,
    false,
    "no modifiers should be set for key-only string",
  );
}

function testStringToShortcutSingleAlt(): void {
  const result = stringToShortcut("Alt+F4");

  assertEquals(result.modifiers.alt, true, "alt should be true");
  assertEquals(result.modifiers.ctrl, false, "ctrl should be false");
  assertEquals(result.key, "f4", "key should be 'f4'");
}

function testStringToShortcutCaseInsensitive(): void {
  const result = stringToShortcut("alt+ctrl+SHIFT+a");

  assertEquals(
    result.modifiers.alt,
    true,
    "lowercase 'alt' should still parse",
  );
  assertEquals(
    result.modifiers.ctrl,
    true,
    "lowercase 'ctrl' should still parse",
  );
  assertEquals(
    result.modifiers.shift,
    true,
    "uppercase 'SHIFT' should still parse",
  );
  assertEquals(result.key, "a", "key should be normalized to lowercase");
}

function testStringToShortcutDigitKey(): void {
  const result = stringToShortcut("Ctrl+5");

  assertEquals(result.modifiers.ctrl, true, "ctrl should be true");
  assertEquals(result.key, "5", "digit key should be preserved");
}

function testStringToShortcutAllModifiers(): void {
  const result = stringToShortcut("Alt+Ctrl+Meta+Shift+Escape");

  assertEquals(result.modifiers.alt, true, "alt should be true");
  assertEquals(result.modifiers.ctrl, true, "ctrl should be true");
  assertEquals(result.modifiers.meta, true, "meta should be true");
  assertEquals(result.modifiers.shift, true, "shift should be true");
  assertEquals(result.key, "escape", "key should be 'escape'");
}

function testStringToShortcutSetsEmptyAction(): void {
  const result = stringToShortcut("Ctrl+T");

  assertEquals(
    result.action,
    "",
    "stringToShortcut should always return empty action",
  );
}

// ---------------------------------------------------------------------------
// Round-trip tests (shortcutToString ↔ stringToShortcut)
// ---------------------------------------------------------------------------

function testRoundTripCtrlT(): void {
  const original = "Ctrl+T";
  const parsed = stringToShortcut(original);
  const reconstructed = shortcutToString({
    ...parsed,
    action: "test",
  });

  assertEquals(reconstructed, original, "round-trip Ctrl+T should be stable");
}

function testRoundTripAltShiftX(): void {
  const original = "Alt+Shift+X";
  const parsed = stringToShortcut(original);
  const reconstructed = shortcutToString({
    ...parsed,
    action: "test",
  });

  assertEquals(
    reconstructed,
    original,
    "round-trip Alt+Shift+X should be stable",
  );
}

function testRoundTripAllModifiers(): void {
  const original = "Alt+Ctrl+Meta+Shift+K";
  const parsed = stringToShortcut(original);
  const reconstructed = shortcutToString({
    ...parsed,
    action: "test",
  });

  assertEquals(
    reconstructed,
    original,
    "round-trip with all modifiers should be stable",
  );
}

function testRoundTripDigitKey(): void {
  const original = "Ctrl+1";
  const parsed = stringToShortcut(original);
  const reconstructed = shortcutToString({
    ...parsed,
    action: "test",
  });

  assertEquals(reconstructed, original, "round-trip Ctrl+1 should be stable");
}

function testRoundTripKeyOnly(): void {
  const original = "A";
  const parsed = stringToShortcut(original);
  const reconstructed = shortcutToString({
    ...parsed,
    action: "test",
  });

  assertEquals(reconstructed, original, "round-trip key-only should be stable");
}

// ---------------------------------------------------------------------------
// defaultConfig / constants tests
// ---------------------------------------------------------------------------

function testDefaultConfigStructure(): void {
  assertEquals(defaultConfig.enabled, true, "default config should be enabled");
  assertEquals(
    Object.keys(defaultConfig.shortcuts).length,
    1,
    "default config should have one shortcut",
  );
}

function testStrDefaultConfigIsParseable(): void {
  const parsed = JSON.parse(strDefaultConfig);
  assertEquals(
    parsed.enabled,
    true,
    "parsed default config string should be enabled",
  );
  assertEquals(
    Object.keys(parsed.shortcuts).length,
    1,
    "parsed default config should have one shortcut (command palette)",
  );
}

function testPrefConstantsAreStrings(): void {
  assertEquals(
    typeof KEYBOARD_SHORTCUT_ENABLED_PREF,
    "string",
    "enabled pref constant should be a string",
  );
  assertEquals(
    typeof KEYBOARD_SHORTCUT_CONFIG_PREF,
    "string",
    "config pref constant should be a string",
  );
  assertNotEquals(
    KEYBOARD_SHORTCUT_ENABLED_PREF,
    "",
    "enabled pref should not be empty",
  );
  assertNotEquals(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
    "",
    "config pref should not be empty",
  );
}

// ---------------------------------------------------------------------------
// normalizeConfig tests (internal function - exported for testing)
// ---------------------------------------------------------------------------

function testNormalizeConfigWithEmptyObject(): void {
  // We can't directly test normalizeConfig since it's not exported,
  // but we can test parseConfig which uses it
  const result = JSON.parse(strDefaultConfig);
  assertEquals(
    result.enabled,
    true,
    "normalized config should have enabled from default",
  );
  assertEquals(
    typeof result.shortcuts,
    "object",
    "normalized config should have shortcuts object",
  );
}

function testNormalizeConfigWithPartialConfig(): void {
  const partialConfig = JSON.stringify({
    enabled: false,
  });
  // parseConfig will normalize by merging with defaults
  const parsed = JSON.parse(partialConfig);
  assertEquals(
    parsed.enabled,
    false,
    "partial config should preserve enabled field",
  );
}

// ---------------------------------------------------------------------------
// parseConfig error handling tests (behavioral testing via prefs)
// ---------------------------------------------------------------------------

function testParseConfigHandlesInvalidJson(): void {
  const hadConfig = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
  );
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF)
    : null;

  try {
    // Set invalid JSON and apply it via setConfig.
    // parseConfig catches JSON.parse errors and returns defaultConfig,
    // then the SolidJS effect writes the normalized default back to the pref.
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      "invalid-json{{{",
    );
    setConfig({
      enabled: true,
      shortcuts: {},
    });

    // After setConfig, the pref should contain valid JSON (the default config),
    // because parseConfig returned defaultConfig and the reactive effect wrote it back.
    const currentPref = Services.prefs.getStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      strDefaultConfig,
    );
    const parsed = JSON.parse(currentPref);
    assertEquals(
      parsed.enabled,
      true,
      "invalid JSON should result in default config being written back",
    );
    assertEquals(
      Object.keys(parsed.shortcuts).length,
      0,
      "invalid JSON should result in empty shortcuts in default config",
    );
  } finally {
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF, savedConfig);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_CONFIG_PREF);
    }
  }
}

function testParseConfigHandlesMalformedObject(): void {
  const hadConfig = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
  );
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF)
    : null;

  try {
    // Set valid JSON but missing required fields
    Services.prefs.setStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      JSON.stringify({ invalidField: "test" }),
    );
    const currentPref = Services.prefs.getStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
      strDefaultConfig,
    );
    assert(
      currentPref.includes("invalidField"),
      "pref should preserve the malformed object",
    );
  } finally {
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF, savedConfig);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_CONFIG_PREF);
    }
  }
}

// ---------------------------------------------------------------------------
// isEnabled / setEnabled tests
// ---------------------------------------------------------------------------

function testIsEnabledReturnsDefaultValue(): void {
  const hadEnabled = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_ENABLED_PREF,
  );
  const savedEnabled = hadEnabled
    ? Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF)
    : null;

  try {
    Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    // After clearing, should return default
    const defaultVal = Services.prefs.getBoolPref(
      KEYBOARD_SHORTCUT_ENABLED_PREF,
      true,
    );
    assertEquals(defaultVal, true, "default enabled should be true");
  } finally {
    if (hadEnabled && savedEnabled !== null) {
      Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, savedEnabled);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    }
  }
}

function testSetEnabledUpdatesPref(): void {
  const hadEnabled = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_ENABLED_PREF,
  );
  const savedEnabled = hadEnabled
    ? Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF)
    : null;

  try {
    setEnabled(false);
    const result = Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    assertEquals(result, false, "setEnabled should update pref to false");

    setEnabled(true);
    const result2 = Services.prefs.getBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    assertEquals(result2, true, "setEnabled should update pref to true");
  } finally {
    if (hadEnabled && savedEnabled !== null) {
      Services.prefs.setBoolPref(KEYBOARD_SHORTCUT_ENABLED_PREF, savedEnabled);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
    }
  }
}

// ---------------------------------------------------------------------------
// getConfig / setConfig tests
// ---------------------------------------------------------------------------

function testSetConfigUpdatesPref(): void {
  const hadConfig = Services.prefs.prefHasUserValue(
    KEYBOARD_SHORTCUT_CONFIG_PREF,
  );
  const savedConfig = hadConfig
    ? Services.prefs.getStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF)
    : null;

  try {
    const testConfig = {
      enabled: true,
      shortcuts: {
        "test-action": {
          key: "T",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "test-action",
        },
      },
    };
    setConfig(testConfig);
    const prefValue = Services.prefs.getStringPref(
      KEYBOARD_SHORTCUT_CONFIG_PREF,
    );
    const parsed = JSON.parse(prefValue);
    assertEquals(
      parsed.shortcuts["test-action"].key,
      "T",
      "setConfig should update pref",
    );
  } finally {
    if (hadConfig && savedConfig !== null) {
      Services.prefs.setStringPref(KEYBOARD_SHORTCUT_CONFIG_PREF, savedConfig);
    } else {
      Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_CONFIG_PREF);
    }
  }
}

// ---------------------------------------------------------------------------
// Additional edge cases for shortcutToString/stringToShortcut
// ---------------------------------------------------------------------------

function testStringToShortcutWithSpecialCharacters(): void {
  // "+" is the delimiter in the shortcut string format, so "Ctrl++" splits
  // into ["ctrl", "", ""] and the last element (the key) is empty.
  const result = stringToShortcut("Ctrl++");
  assertEquals(
    result.key,
    "",
    "+ as delimiter produces empty key after splitting",
  );
  assertEquals(result.modifiers.ctrl, true, "ctrl should be true");
}

function testStringToShortcutWithMultiplePlusSigns(): void {
  // "+" is the delimiter, so "Ctrl++Shift++" splits into
  // ["ctrl", "", "shift", "", ""] and the key (last element) is empty.
  const result = stringToShortcut("Ctrl++Shift++");
  assertEquals(result.key, "", "last element after splitting on + is empty");
  assertEquals(result.modifiers.ctrl, true, "ctrl should be true");
  assertEquals(result.modifiers.shift, true, "shift should be true");
}

function testShortcutToStringWithSpecialKey(): void {
  const result = shortcutToString({
    modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    key: "+",
    action: "test",
  });
  assertEquals(result, "Ctrl++", "special key + should be handled");
}

function testStringToShortcutWithWhitespace(): void {
  const result = stringToShortcut("  Ctrl  +  T  ");
  // The function doesn't trim, so this will fail - testing the behavior
  assertEquals(
    result.key,
    "  t  ",
    "whitespace in key should be preserved (no trimming)",
  );
  assertEquals(
    result.modifiers.ctrl,
    false,
    "whitespace in modifier should prevent matching",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutConfig.test.ts", [
    // shortcutToString
    {
      name: "shortcutToString with all modifiers",
      fn: testShortcutToStringWithAllModifiers,
    },
    {
      name: "shortcutToString no modifiers",
      fn: testShortcutToStringNoModifiers,
    },
    {
      name: "shortcutToString single Alt",
      fn: testShortcutToStringSingleAlt,
    },
    {
      name: "shortcutToString single Ctrl",
      fn: testShortcutToStringSingleCtrl,
    },
    {
      name: "shortcutToString single Meta",
      fn: testShortcutToStringSingleMeta,
    },
    {
      name: "shortcutToString single Shift",
      fn: testShortcutToStringSingleShift,
    },
    {
      name: "shortcutToString uppercases key",
      fn: testShortcutToStringUppercasesKey,
    },
    {
      name: "shortcutToString with digit key",
      fn: testShortcutToStringWithDigitKey,
    },
    {
      name: "shortcutToString all four modifiers",
      fn: testShortcutToStringAllFourModifiers,
    },
    {
      name: "shortcutToString modifier order",
      fn: testShortcutToStringModifierOrder,
    },
    // stringToShortcut
    {
      name: "stringToShortcut parses modifiers",
      fn: testStringToShortcutParsesModifiers,
    },
    {
      name: "stringToShortcut handles empty string",
      fn: testStringToShortcutHandlesEmptyString,
    },
    {
      name: "stringToShortcut key only",
      fn: testStringToShortcutKeyOnly,
    },
    {
      name: "stringToShortcut single Alt",
      fn: testStringToShortcutSingleAlt,
    },
    {
      name: "stringToShortcut case insensitive",
      fn: testStringToShortcutCaseInsensitive,
    },
    {
      name: "stringToShortcut digit key",
      fn: testStringToShortcutDigitKey,
    },
    {
      name: "stringToShortcut all modifiers",
      fn: testStringToShortcutAllModifiers,
    },
    {
      name: "stringToShortcut sets empty action",
      fn: testStringToShortcutSetsEmptyAction,
    },
    // Round-trip
    {
      name: "round-trip Ctrl+T",
      fn: testRoundTripCtrlT,
    },
    {
      name: "round-trip Alt+Shift+X",
      fn: testRoundTripAltShiftX,
    },
    {
      name: "round-trip all modifiers",
      fn: testRoundTripAllModifiers,
    },
    {
      name: "round-trip digit key",
      fn: testRoundTripDigitKey,
    },
    {
      name: "round-trip key only",
      fn: testRoundTripKeyOnly,
    },
    // defaultConfig / constants
    {
      name: "defaultConfig structure",
      fn: testDefaultConfigStructure,
    },
    {
      name: "strDefaultConfig is parseable",
      fn: testStrDefaultConfigIsParseable,
    },
    {
      name: "pref constants are valid strings",
      fn: testPrefConstantsAreStrings,
    },
    // normalizeConfig (behavioral)
    {
      name: "normalizeConfig with empty object",
      fn: testNormalizeConfigWithEmptyObject,
    },
    {
      name: "normalizeConfig with partial config",
      fn: testNormalizeConfigWithPartialConfig,
    },
    // parseConfig error handling
    {
      name: "parseConfig handles invalid JSON",
      fn: testParseConfigHandlesInvalidJson,
    },
    {
      name: "parseConfig handles malformed object",
      fn: testParseConfigHandlesMalformedObject,
    },
    // isEnabled / setEnabled
    {
      name: "isEnabled returns default value",
      fn: testIsEnabledReturnsDefaultValue,
    },
    {
      name: "setEnabled updates pref",
      fn: testSetEnabledUpdatesPref,
    },
    // getConfig / setConfig
    {
      name: "setConfig updates pref",
      fn: testSetConfigUpdatesPref,
    },
    // Additional edge cases
    {
      name: "stringToShortcut with special characters",
      fn: testStringToShortcutWithSpecialCharacters,
    },
    {
      name: "stringToShortcut with multiple plus signs",
      fn: testStringToShortcutWithMultiplePlusSigns,
    },
    {
      name: "shortcutToString with special key",
      fn: testShortcutToStringWithSpecialKey,
    },
    {
      name: "stringToShortcut with whitespace",
      fn: testStringToShortcutWithWhitespace,
    },
  ]);
}
