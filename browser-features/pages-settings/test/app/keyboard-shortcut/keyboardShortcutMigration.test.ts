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
  const macZen = mac.shortcuts[ZEN_MODE_ACTION]?.[0];
  assertEquals(mac.schemaVersion, 3, "fresh mac config is v3");
  assertEquals(macZen.key, "KeyZ", "fresh mac config stores physical code");
  assertEquals(macZen.modifiers.alt, true, "mac Zen uses Alt");
  assertEquals(macZen.modifiers.meta, true, "mac Zen uses Meta");
  assertEquals(macZen.modifiers.ctrl, false, "mac Zen excludes Ctrl");

  const windows = createDefaultKeyboardShortcutConfig(true, "win");
  const windowsZen = windows.shortcuts[ZEN_MODE_ACTION]?.[0];
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

  assertEquals(migrated.schemaVersion, 3, "legacy settings migrate to v3");
  assertEquals(migrated.enabled, false, "separate enabled pref wins");
  assertEquals(
    JSON.stringify(migrated.shortcuts["floorp-toggle-command-palette"]),
    JSON.stringify([rebound]),
    "rebound shortcut is preserved exactly",
  );
  assert(
    Object.hasOwn(migrated.shortcuts, ZEN_MODE_ACTION),
    "legacy settings receive Zen once",
  );
}

function testV2SingleBindingMigratesToBindingArray(): void {
  const migrated = migrateKeyboardShortcutConfig(
    {
      schemaVersion: 2,
      enabled: true,
      shortcuts: {
        "gecko-back": {
          key: "KeyB",
          modifiers: { alt: false, ctrl: true, meta: false, shift: false },
          action: "gecko-back",
        },
      },
    },
    true,
    "win",
  );

  assertEquals(migrated.schemaVersion, 3, "v2 config migrates to v3");
  assertEquals(
    migrated.shortcuts["gecko-back"]?.length,
    1,
    "v2 binding should become one array entry",
  );
  assertEquals(
    migrated.shortcuts["gecko-back"]?.[0].key,
    "KeyB",
    "binding key should be preserved",
  );
  assertEquals(
    migrated.shortcuts["gecko-back"]?.[0].action,
    "gecko-back",
    "binding action should match its record key",
  );
}

function testMultipleBindingsArePreservedAndDeduplicated(): void {
  const migrated = migrateKeyboardShortcutConfig(
    {
      schemaVersion: 3,
      enabled: true,
      shortcuts: {
        "gecko-back": [
          {
            key: "KeyB",
            modifiers: { alt: false, ctrl: true, meta: false, shift: false },
            action: "gecko-back",
          },
          {
            key: "B",
            modifiers: { alt: false, ctrl: true, meta: false, shift: false },
            action: "gecko-back",
          },
          {
            key: "ArrowLeft",
            modifiers: { alt: true, ctrl: false, meta: false, shift: false },
            action: "gecko-back",
          },
        ],
      },
    },
    true,
    "win",
  );

  assertEquals(
    migrated.shortcuts["gecko-back"]?.length,
    2,
    "equivalent bindings should be deduplicated",
  );
  assertEquals(
    migrated.shortcuts["gecko-back"]?.[1].key,
    "ArrowLeft",
    "distinct binding should remain",
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
    schemaVersion: 3 as const,
    enabled: true,
    shortcuts: {
      "floorp-toggle-command-palette": [{
        key: "F2",
        modifiers: { alt: false, ctrl: false, meta: false, shift: false },
        action: "floorp-toggle-command-palette",
      }],
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
    "settings save persists schema v3",
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
  assertEquals(recovered.schemaVersion, 3, "invalid JSON recovers to v3");
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
    name: "v2 binding migrates to binding array",
    fn: testV2SingleBindingMigratesToBindingArray,
  },
  {
    name: "multiple bindings are preserved and deduplicated",
    fn: testMultipleBindingsArePreservedAndDeduplicated,
  },
  {
    name: "legacy migration preserves existing Zen",
    fn: testExistingZenBindingIsPreserved,
  },
  {
    name: "v3 deletion survives round trip",
    fn: testV2DeletionSurvivesRoundTrip,
  },
  {
    name: "invalid JSON recovers to v3 defaults",
    fn: testInvalidJsonRecoversToV2Defaults,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutMigration.test.ts", tests);
}
