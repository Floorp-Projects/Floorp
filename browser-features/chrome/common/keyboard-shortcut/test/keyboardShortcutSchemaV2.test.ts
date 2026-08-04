// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  createDefaultConfig,
  createZenModeShortcut,
  KEYBOARD_SHORTCUT_SCHEMA_VERSION,
  ZEN_MODE_ACTION,
} from "../defaults.ts";
import { migrateConfigToV2 } from "../migration.ts";

function testPlatformDefaults(): void {
  const mac = createZenModeShortcut("macosx");
  assertEquals(mac.key, "KeyZ", "mac default stores physical code");
  assertEquals(mac.modifiers.alt, true, "mac default uses Alt");
  assertEquals(mac.modifiers.ctrl, false, "mac default excludes Ctrl");
  assertEquals(mac.modifiers.meta, true, "mac default uses Meta");

  const windows = createZenModeShortcut("win");
  assertEquals(windows.key, "KeyZ", "non-mac default stores physical code");
  assertEquals(windows.modifiers.alt, true, "non-mac default uses Alt");
  assertEquals(windows.modifiers.ctrl, true, "non-mac default uses Ctrl");
  assertEquals(windows.modifiers.meta, false, "non-mac default excludes Meta");
}

function testFreshDefaultsAreV2(): void {
  const defaults = createDefaultConfig("win");
  assertEquals(
    defaults.schemaVersion,
    KEYBOARD_SHORTCUT_SCHEMA_VERSION,
    "fresh defaults use schema v2",
  );
  assertEquals(
    Object.keys(defaults.shortcuts).length,
    2,
    "fresh defaults include F2 and Zen",
  );
  assert(
    Object.hasOwn(defaults.shortcuts, ZEN_MODE_ACTION),
    "fresh defaults include Zen",
  );
}

function testLegacyAddsOnlyZen(): void {
  const rebound = {
    key: "KeyP",
    modifiers: { alt: false, ctrl: true, meta: false, shift: true },
    action: "floorp-toggle-command-palette",
  };
  const migrated = migrateConfigToV2(
    {
      enabled: false,
      shortcuts: { "floorp-toggle-command-palette": rebound },
    },
    createDefaultConfig("win"),
  );

  assertEquals(migrated.schemaVersion, 2, "legacy config migrates to v2");
  assertEquals(migrated.enabled, false, "legacy enabled state is preserved");
  assertEquals(
    JSON.stringify(migrated.shortcuts["floorp-toggle-command-palette"]),
    JSON.stringify(rebound),
    "legacy rebound shortcut is preserved exactly",
  );
  assertEquals(
    Object.keys(migrated.shortcuts).length,
    2,
    "migration adds Zen without restoring other defaults",
  );
}

function testExistingZenBindingIsPreserved(): void {
  const reboundZen = {
    key: "BracketRight",
    modifiers: { alt: false, ctrl: true, meta: false, shift: true },
    action: ZEN_MODE_ACTION,
  };
  const migrated = migrateConfigToV2(
    { shortcuts: { "custom-zen-binding": reboundZen } },
    createDefaultConfig("win"),
  );

  assertEquals(
    JSON.stringify(migrated.shortcuts["custom-zen-binding"]),
    JSON.stringify(reboundZen),
    "an existing Zen binding under a custom id is not overwritten",
  );
  assertEquals(
    Object.keys(migrated.shortcuts).length,
    1,
    "an existing Zen action does not receive a duplicate default binding",
  );
}

function testV2DeletionPersists(): void {
  const deleted = {
    schemaVersion: 2,
    enabled: true,
    shortcuts: {},
  } as const;
  const firstLoad = migrateConfigToV2(deleted, createDefaultConfig("win"));
  const restartLoad = migrateConfigToV2(
    JSON.parse(JSON.stringify(firstLoad)),
    createDefaultConfig("win"),
  );

  assertEquals(
    Object.hasOwn(firstLoad.shortcuts, ZEN_MODE_ACTION),
    false,
    "v2 deletion is preserved on load",
  );
  assertEquals(
    Object.hasOwn(restartLoad.shortcuts, ZEN_MODE_ACTION),
    false,
    "v2 deletion is preserved after restart serialization",
  );
}

const tests: TestCase[] = [
  { name: "platform defaults", fn: testPlatformDefaults },
  { name: "fresh defaults are v2", fn: testFreshDefaultsAreV2 },
  { name: "legacy migration adds only Zen", fn: testLegacyAddsOnlyZen },
  {
    name: "legacy migration preserves existing Zen binding",
    fn: testExistingZenBindingIsPreserved,
  },
  { name: "v2 deletion persists", fn: testV2DeletionPersists },
];

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutSchemaV2.test.ts", tests);
}
