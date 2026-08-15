// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  createDefaultKeyboardShortcutConfig,
  migrateKeyboardShortcutConfig,
  parseKeyboardShortcutConfig,
  serializeKeyboardShortcutConfig,
  ZEN_MODE_ACTION,
} from "../../../src/types/pref.ts";

function testFreshPlatformDefaults(): void {
  const mac = createDefaultKeyboardShortcutConfig(true, "macosx");
  const macZen = mac.shortcuts[ZEN_MODE_ACTION];
  assertEquals(mac.schemaVersion, 2, "fresh mac config is v2");
  assertEquals(macZen.key, "KeyZ", "fresh mac config stores physical code");
  assertEquals(macZen.modifiers.alt, true, "mac Zen uses Alt");
  assertEquals(macZen.modifiers.meta, true, "mac Zen uses Meta");
  assertEquals(macZen.modifiers.ctrl, false, "mac Zen excludes Ctrl");

  const windows = createDefaultKeyboardShortcutConfig(true, "win");
  const windowsZen = windows.shortcuts[ZEN_MODE_ACTION];
  assertEquals(windowsZen.modifiers.alt, true, "non-mac Zen uses Alt");
  assertEquals(windowsZen.modifiers.ctrl, true, "non-mac Zen uses Ctrl");
  assertEquals(windowsZen.modifiers.meta, false, "non-mac Zen excludes Meta");
}

function testLegacyMigrationPreservesRebinding(): void {
  const rebound = {
    key: "KeyP",
    modifiers: { alt: false, ctrl: true, meta: false, shift: true },
    action: "floorp-toggle-command-palette",
  };
  const migrated = migrateKeyboardShortcutConfig(
    { shortcuts: { "floorp-toggle-command-palette": rebound } },
    false,
    "win",
  );

  assertEquals(migrated.schemaVersion, 2, "legacy settings migrate to v2");
  assertEquals(migrated.enabled, false, "separate enabled pref wins");
  assertEquals(
    JSON.stringify(migrated.shortcuts["floorp-toggle-command-palette"]),
    JSON.stringify(rebound),
    "rebound shortcut is preserved exactly",
  );
  assert(
    Object.hasOwn(migrated.shortcuts, ZEN_MODE_ACTION),
    "legacy settings receive Zen once",
  );
}

function testExistingZenBindingIsPreserved(): void {
  const reboundZen = {
    key: "Semicolon",
    modifiers: { alt: false, ctrl: true, meta: false, shift: true },
    action: ZEN_MODE_ACTION,
  };
  const migrated = migrateKeyboardShortcutConfig(
    { shortcuts: { "custom-zen-binding": reboundZen } },
    true,
    "win",
  );

  assertEquals(
    JSON.stringify(migrated.shortcuts["custom-zen-binding"]),
    JSON.stringify(reboundZen),
    "legacy migration does not overwrite an existing Zen binding under a custom id",
  );
  assertEquals(
    Object.keys(migrated.shortcuts).length,
    1,
    "settings migration does not add a duplicate default Zen binding",
  );
}

function testV2DeletionSurvivesRoundTrip(): void {
  const deleted = {
    schemaVersion: 2 as const,
    enabled: true,
    shortcuts: {
      "floorp-toggle-command-palette": {
        key: "F2",
        modifiers: { alt: false, ctrl: false, meta: false, shift: false },
        action: "floorp-toggle-command-palette",
      },
    },
  };
  const saved = serializeKeyboardShortcutConfig(deleted);
  const reopened = parseKeyboardShortcutConfig(saved, true, "win");
  const restarted = migrateKeyboardShortcutConfig(
    JSON.parse(serializeKeyboardShortcutConfig(reopened)),
    true,
    "win",
  );

  assertEquals(
    JSON.parse(saved).schemaVersion,
    2,
    "settings save persists schema v2",
  );
  assertEquals(
    Object.hasOwn(reopened.shortcuts, ZEN_MODE_ACTION),
    false,
    "settings reopen preserves Zen deletion",
  );
  assertEquals(
    Object.hasOwn(restarted.shortcuts, ZEN_MODE_ACTION),
    false,
    "browser restart preserves Zen deletion",
  );
}

function testInvalidJsonRecoversToV2Defaults(): void {
  const recovered = parseKeyboardShortcutConfig("not-json{{", true, "win");
  assertEquals(recovered.schemaVersion, 2, "invalid JSON recovers to v2");
  assertEquals(
    Object.keys(recovered.shortcuts).length,
    2,
    "invalid JSON recovers to F2 and Zen defaults",
  );
}

const tests: TestCase[] = [
  { name: "fresh platform defaults", fn: testFreshPlatformDefaults },
  {
    name: "legacy migration preserves rebinding",
    fn: testLegacyMigrationPreservesRebinding,
  },
  {
    name: "legacy migration preserves existing Zen",
    fn: testExistingZenBindingIsPreserved,
  },
  {
    name: "v2 deletion survives round trip",
    fn: testV2DeletionSurvivesRoundTrip,
  },
  {
    name: "invalid JSON recovers to v2 defaults",
    fn: testInvalidJsonRecoversToV2Defaults,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutMigration.test.ts", tests);
}
