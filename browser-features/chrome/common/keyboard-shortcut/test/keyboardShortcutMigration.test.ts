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
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
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
