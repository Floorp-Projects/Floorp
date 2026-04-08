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
