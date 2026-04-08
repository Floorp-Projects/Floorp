// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import { shortcutToString, stringToShortcut } from "../config.ts";

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

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutConfig.test.ts", [
    {
      name: "shortcutToString with all modifiers",
      fn: testShortcutToStringWithAllModifiers,
    },
    {
      name: "stringToShortcut parses modifiers",
      fn: testStringToShortcutParsesModifiers,
    },
    {
      name: "stringToShortcut handles empty string",
      fn: testStringToShortcutHandlesEmptyString,
    },
  ]);
}
