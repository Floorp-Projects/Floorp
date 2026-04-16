// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { migrateLegacyConfig, clearLegacyConfig } from "../migration.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

const LEGACY_PREF = "floorp.browser.nora.csk.data";

function withStringPref(value: string, fn: () => void): void {
  Services.prefs.setStringPref(LEGACY_PREF, value);
  try {
    fn();
  } finally {
    Services.prefs.clearUserPref(LEGACY_PREF);
  }
}

// ---------------------------------------------------------------------------
// Tests — migrateLegacyConfig
// ---------------------------------------------------------------------------

function testNoPrefReturnsNull(): void {
  // Ensure pref does not exist
  if (Services.prefs.prefHasUserValue(LEGACY_PREF)) {
    Services.prefs.clearUserPref(LEGACY_PREF);
  }
  assertEquals(
    migrateLegacyConfig(),
    null,
    "should return null when pref not set",
  );
}

function testEmptyObjectReturnsEmptyShortcuts(): void {
  withStringPref("{}", () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null for empty object");
    assertEquals(result.enabled, true, "enabled should be true");
    assertEquals(
      Object.keys(result.shortcuts).length,
      0,
      "shortcuts should be empty",
    );
  });
}

function testSingleShortcutMigration(): void {
  const legacy = {
    "open-new-tab": {
      key: "t",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    const shortcut = result.shortcuts["open-new-tab"];
    assert(shortcut !== undefined, "should have open-new-tab shortcut");
    assertEquals(shortcut.key, "t", "key should be t");
    assertEquals(shortcut.action, "open-new-tab", "action should match");
    assertEquals(shortcut.modifiers.ctrl, true, "ctrl should be true");
    assertEquals(shortcut.modifiers.alt, false, "alt should be false");
  });
}

function testMultipleShortcuts(): void {
  const legacy = {
    action1: {
      key: "a",
      modifiers: { alt: true, ctrl: false, meta: false, shift: false },
    },
    action2: {
      key: "b",
      modifiers: { alt: false, ctrl: false, meta: true, shift: true },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      2,
      "should have two shortcuts",
    );
  });
}

function testInvalidJsonReturnsNull(): void {
  withStringPref("not-valid-json{{{", () => {
    const result = migrateLegacyConfig();
    assertEquals(result, null, "invalid JSON should return null");
  });
}

function testNonObjectEntrySkipped(): void {
  const legacy = {
    validAction: {
      key: "x",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
    invalidAction: "not-an-object",
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      1,
      "should only have the valid shortcut",
    );
    assert(
      result.shortcuts["validAction"] !== undefined,
      "validAction should exist",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests — clearLegacyConfig
// ---------------------------------------------------------------------------

function testClearLegacyConfig(): void {
  Services.prefs.setStringPref(LEGACY_PREF, "{}");
  assert(
    Services.prefs.prefHasUserValue(LEGACY_PREF),
    "pref should exist before clear",
  );
  clearLegacyConfig();
  assertEquals(
    Services.prefs.prefHasUserValue(LEGACY_PREF),
    false,
    "pref should be cleared",
  );
}

function testClearLegacyConfigNoOp(): void {
  // Should not throw when pref doesn't exist
  if (Services.prefs.prefHasUserValue(LEGACY_PREF)) {
    Services.prefs.clearUserPref(LEGACY_PREF);
  }
  clearLegacyConfig(); // should be a no-op without error
}

// ---------------------------------------------------------------------------
// Additional edge-case tests
// ---------------------------------------------------------------------------

function testNullEntrySkipped(): void {
  const legacy = {
    validAction: {
      key: "z",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
    nullAction: null as unknown as {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      1,
      "null entries should be skipped, only valid one remains",
    );
    assert(
      result.shortcuts["validAction"] !== undefined,
      "validAction should exist",
    );
  });
}

function testAllModifiersEnabled(): void {
  const legacy = {
    superAction: {
      key: "s",
      modifiers: { alt: true, ctrl: true, meta: true, shift: true },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    const shortcut = result.shortcuts["superAction"];
    assert(shortcut !== undefined, "superAction should exist");
    assertEquals(shortcut.modifiers.alt, true, "alt should be true");
    assertEquals(shortcut.modifiers.ctrl, true, "ctrl should be true");
    assertEquals(shortcut.modifiers.meta, true, "meta should be true");
    assertEquals(shortcut.modifiers.shift, true, "shift should be true");
    assertEquals(shortcut.key, "s", "key should be s");
  });
}

function testDigitKeyMigration(): void {
  const legacy = {
    tabSwitch1: {
      key: "1",
      modifiers: { alt: true, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    const shortcut = result.shortcuts["tabSwitch1"];
    assert(shortcut !== undefined, "tabSwitch1 should exist");
    assertEquals(shortcut.key, "1", "digit key should be preserved");
  });
}

function testEmptyKeyMigration(): void {
  const legacy = {
    emptyKey: {
      key: "",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    const shortcut = result.shortcuts["emptyKey"];
    assert(shortcut !== undefined, "emptyKey should exist");
    assertEquals(shortcut.key, "", "empty key should be preserved");
  });
}

function testNumericEntrySkipped(): void {
  const legacy = {
    numericAction: 42 as unknown as {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      0,
      "numeric entries should be skipped",
    );
  });
}

function testBooleanEntrySkipped(): void {
  const legacy = {
    boolAction: true as unknown as {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      0,
      "boolean entries should be skipped",
    );
  });
}

function testActionFieldIsSetCorrectly(): void {
  const legacy = {
    "my-custom-action": {
      key: "m",
      modifiers: { alt: true, ctrl: false, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    const shortcut = result.shortcuts["my-custom-action"];
    assert(shortcut !== undefined, "my-custom-action should exist");
    assertEquals(
      shortcut.action,
      "my-custom-action",
      "action field should match the key name from legacy config",
    );
  });
}

function testEmptyObjectProducesEnabledTrue(): void {
  withStringPref("{}", () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      result.enabled,
      true,
      "enabled should always be true in migrated config",
    );
  });
}

function testLargeNumberOfShortcuts(): void {
  const legacy: Record<
    string,
    {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    }
  > = {};
  for (let i = 0; i < 50; i++) {
    legacy[`action-${i}`] = {
      key: String.fromCharCode(97 + (i % 26)),
      modifiers: {
        alt: i % 2 === 0,
        ctrl: i % 3 === 0,
        meta: false,
        shift: i % 5 === 0,
      },
    };
  }
  withStringPref(JSON.stringify(legacy), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not be null");
    assertEquals(
      Object.keys(result.shortcuts).length,
      50,
      "should migrate all 50 shortcuts",
    );
  });
}

function testDeeplyNestedInvalidJson(): void {
  withStringPref("[1,2,3]", () => {
    // Array is valid JSON but not an object — should result in no shortcuts
    const result = migrateLegacyConfig();
    // An array won't have Object.entries yielding action entries, so shortcuts should be empty
    assert(result !== null, "should not be null for array JSON");
    assertEquals(
      Object.keys(result.shortcuts).length,
      0,
      "array JSON should produce empty shortcuts",
    );
  });
}

// ---------------------------------------------------------------------------
// Additional edge case tests for clearLegacyConfig
// ---------------------------------------------------------------------------

function testClearLegacyConfigWithComplexValue(): void {
  const complexValue = {
    "complex-action": {
      key: "c",
      modifiers: {
        alt: true,
        ctrl: true,
        meta: true,
        shift: true,
      },
    },
    "simple-action": {
      key: "s",
      modifiers: {
        alt: false,
        ctrl: true,
        meta: false,
        shift: false,
      },
    },
  };
  withStringPref(JSON.stringify(complexValue), () => {
    assert(
      Services.prefs.prefHasUserValue(LEGACY_PREF),
      "pref should exist before clear",
    );
    clearLegacyConfig();
    assertEquals(
      Services.prefs.prefHasUserValue(LEGACY_PREF),
      false,
      "pref should be cleared after clearLegacyConfig",
    );
  });
}

function testClearLegacyConfigWithUnicodeKey(): void {
  const unicodeValue = {
    "unicode-action": {
      key: "Ω", // Unicode character
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(unicodeValue), () => {
    clearLegacyConfig();
    assertEquals(
      Services.prefs.prefHasUserValue(LEGACY_PREF),
      false,
      "pref with unicode should be cleared",
    );
  });
}

// ---------------------------------------------------------------------------
// Additional edge case tests for migrateLegacyConfig
// ---------------------------------------------------------------------------

function testMigrateWithMissingModifiersField(): void {
  const incompleteEntry = {
    "action-without-modifiers": {
      key: "k",
      // modifiers field missing
    } as unknown as {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    },
  };
  withStringPref(JSON.stringify(incompleteEntry), () => {
    const result = migrateLegacyConfig();
    // Should handle gracefully - either skip or treat missing fields as falsy
    assert(result !== null, "should not crash on missing modifiers");
  });
}

function testMigrateWithUndefinedModifiers(): void {
  const undefinedModifiers = {
    "action-with-undefined-modifiers": {
      key: "u",
      modifiers: undefined,
    } as unknown as {
      key: string;
      modifiers: { alt: boolean; ctrl: boolean; meta: boolean; shift: boolean };
    },
  };
  withStringPref(JSON.stringify(undefinedModifiers), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on undefined modifiers");
  });
}

function testMigrateWithArrayKey(): void {
  const arrayKey = {
    "action-with-array-key": {
      key: ["a", "b", "c"] as unknown as string,
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(arrayKey), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on array key");
    // The migration code copies config.key as-is without conversion,
    // so an array key remains an array (typeof is "object").
    if (result.shortcuts["action-with-array-key"]) {
      assertEquals(
        typeof result.shortcuts["action-with-array-key"].key,
        "object",
        "array key is preserved as-is (not converted to string)",
      );
    }
  });
}

function testMigrateWithObjectKey(): void {
  const objectKey = {
    "action-with-object-key": {
      key: { k: "value" } as unknown as string,
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(objectKey), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on object key");
  });
}

function testMigrateWithNumericStringKey(): void {
  const numericKey = {
    "action-123": {
      key: "123",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(numericKey), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on numeric string key");
    const shortcut = result.shortcuts["action-123"];
    if (shortcut) {
      assertEquals(shortcut.key, "123", "numeric string key should be preserved");
    }
  });
}

function testMigrateWithWhitespaceKey(): void {
  const whitespaceKey = {
    "action-with-whitespace": {
      key: "  space  ",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(whitespaceKey), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on whitespace key");
    const shortcut = result.shortcuts["action-with-whitespace"];
    if (shortcut) {
      assertEquals(shortcut.key, "  space  ", "whitespace in key should be preserved");
    }
  });
}

function testMigrateWithSpecialActionName(): void {
  const specialAction = {
    "action-with-special-chars-!@#$%^&*()": {
      key: "x",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(specialAction), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on special action name");
    const shortcut = result.shortcuts["action-with-special-chars-!@#$%^&*()"];
    if (shortcut) {
      assertEquals(
        shortcut.action,
        "action-with-special-chars-!@#$%^&*()",
        "special action name should be preserved",
      );
    }
  });
}

function testMigrateWithNumericActionName(): void {
  const numericAction = {
    "123": {
      key: "n",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(numericAction), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on numeric action name");
    const shortcut = result.shortcuts["123"];
    if (shortcut) {
      assertEquals(shortcut.action, "123", "numeric action name should be preserved");
    }
  });
}

function testMigratePreservesModifierState(): void {
  const allCombinations = {
    "alt-only": {
      key: "a",
      modifiers: { alt: true, ctrl: false, meta: false, shift: false },
    },
    "ctrl-only": {
      key: "c",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
    "meta-only": {
      key: "m",
      modifiers: { alt: false, ctrl: false, meta: true, shift: false },
    },
    "shift-only": {
      key: "s",
      modifiers: { alt: false, ctrl: false, meta: false, shift: true },
    },
    "alt-ctrl": {
      key: "ac",
      modifiers: { alt: true, ctrl: true, meta: false, shift: false },
    },
    "alt-meta": {
      key: "am",
      modifiers: { alt: true, ctrl: false, meta: true, shift: false },
    },
    "alt-shift": {
      key: "as",
      modifiers: { alt: true, ctrl: false, meta: false, shift: true },
    },
    "ctrl-meta": {
      key: "cm",
      modifiers: { alt: false, ctrl: true, meta: true, shift: false },
    },
    "ctrl-shift": {
      key: "cs",
      modifiers: { alt: false, ctrl: true, meta: false, shift: true },
    },
    "meta-shift": {
      key: "ms",
      modifiers: { alt: false, ctrl: false, meta: true, shift: true },
    },
  };
  withStringPref(JSON.stringify(allCombinations), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on all modifier combinations");

    // Verify a few key combinations
    assertEquals(
      result.shortcuts["alt-only"].modifiers.alt,
      true,
      "alt-only should preserve alt",
    );
    assertEquals(
      result.shortcuts["ctrl-only"].modifiers.ctrl,
      true,
      "ctrl-only should preserve ctrl",
    );
    assertEquals(
      result.shortcuts["alt-ctrl"].modifiers.alt,
      true,
      "alt-ctrl should preserve alt",
    );
    assertEquals(
      result.shortcuts["alt-ctrl"].modifiers.ctrl,
      true,
      "alt-ctrl should preserve ctrl",
    );
    assertEquals(
      result.shortcuts["meta-shift"].modifiers.meta,
      true,
      "meta-shift should preserve meta",
    );
    assertEquals(
      result.shortcuts["meta-shift"].modifiers.shift,
      true,
      "meta-shift should preserve shift",
    );
  });
}

function testMigrateWithVeryLongKey(): void {
  const longKey = {
    "long-key-action": {
      key: "a".repeat(1000),
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(longKey), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on very long key");
    const shortcut = result.shortcuts["long-key-action"];
    if (shortcut) {
      assertEquals(
        shortcut.key.length,
        1000,
        "very long key should be preserved",
      );
    }
  });
}

function testMigrateWithVeryLongActionName(): void {
  const longActionName = "action-".repeat(100) + "end";
  const longAction = {
    [longActionName]: {
      key: "l",
      modifiers: { alt: false, ctrl: true, meta: false, shift: false },
    },
  };
  withStringPref(JSON.stringify(longAction), () => {
    const result = migrateLegacyConfig();
    assert(result !== null, "should not crash on very long action name");
    const shortcut = result.shortcuts[longActionName];
    if (shortcut) {
      assertEquals(
        shortcut.action,
        longActionName,
        "very long action name should be preserved",
      );
    }
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "no pref → null", fn: testNoPrefReturnsNull },
    {
      name: "empty object → empty shortcuts",
      fn: testEmptyObjectReturnsEmptyShortcuts,
    },
    { name: "single shortcut migration", fn: testSingleShortcutMigration },
    { name: "multiple shortcuts", fn: testMultipleShortcuts },
    { name: "invalid JSON → null", fn: testInvalidJsonReturnsNull },
    { name: "non-object entry skipped", fn: testNonObjectEntrySkipped },
    { name: "clearLegacyConfig", fn: testClearLegacyConfig },
    { name: "clearLegacyConfig no-op", fn: testClearLegacyConfigNoOp },
    // Edge cases
    { name: "null entry skipped", fn: testNullEntrySkipped },
    { name: "all modifiers enabled", fn: testAllModifiersEnabled },
    { name: "digit key migration", fn: testDigitKeyMigration },
    { name: "empty key migration", fn: testEmptyKeyMigration },
    { name: "numeric entry skipped", fn: testNumericEntrySkipped },
    { name: "boolean entry skipped", fn: testBooleanEntrySkipped },
    { name: "action field set correctly", fn: testActionFieldIsSetCorrectly },
    {
      name: "empty object produces enabled true",
      fn: testEmptyObjectProducesEnabledTrue,
    },
    { name: "large number of shortcuts", fn: testLargeNumberOfShortcuts },
    {
      name: "deeply nested invalid JSON (array)",
      fn: testDeeplyNestedInvalidJson,
    },
    // Additional clearLegacyConfig tests
    {
      name: "clearLegacyConfig with complex value",
      fn: testClearLegacyConfigWithComplexValue,
    },
    {
      name: "clearLegacyConfig with unicode key",
      fn: testClearLegacyConfigWithUnicodeKey,
    },
    // Additional migrateLegacyConfig tests
    {
      name: "migrate with missing modifiers field",
      fn: testMigrateWithMissingModifiersField,
    },
    {
      name: "migrate with undefined modifiers",
      fn: testMigrateWithUndefinedModifiers,
    },
    {
      name: "migrate with array key",
      fn: testMigrateWithArrayKey,
    },
    {
      name: "migrate with object key",
      fn: testMigrateWithObjectKey,
    },
    {
      name: "migrate with numeric string key",
      fn: testMigrateWithNumericStringKey,
    },
    {
      name: "migrate with whitespace key",
      fn: testMigrateWithWhitespaceKey,
    },
    {
      name: "migrate with special action name",
      fn: testMigrateWithSpecialActionName,
    },
    {
      name: "migrate with numeric action name",
      fn: testMigrateWithNumericActionName,
    },
    {
      name: "migrate preserves modifier state",
      fn: testMigratePreservesModifierState,
    },
    {
      name: "migrate with very long key",
      fn: testMigrateWithVeryLongKey,
    },
    {
      name: "migrate with very long action name",
      fn: testMigrateWithVeryLongActionName,
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `keyboardShortcutMigration.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
