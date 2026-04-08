// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assertNotEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  shortcutToString,
  stringToShortcut,
  defaultConfig,
  strDefaultConfig,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  KEYBOARD_SHORTCUT_CONFIG_PREF,
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
    0,
    "default config should have no shortcuts",
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
    0,
    "parsed default config should have empty shortcuts",
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
  ]);
}
