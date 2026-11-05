// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import {
  normalizeTabbarStyle,
  resolveTabbarStyleFromNative,
} from "../utils/tabbar-style-sync.ts";

function testVerticalOnAlwaysVertical(): void {
  assertEquals(
    resolveTabbarStyleFromNative(true, "horizontal"),
    "vertical",
    "horizontal becomes vertical",
  );
  assertEquals(
    resolveTabbarStyleFromNative(true, "multirow"),
    "vertical",
    "multirow becomes vertical",
  );
  assertEquals(
    resolveTabbarStyleFromNative(true, "vertical"),
    "vertical",
    "vertical stays vertical",
  );
}

function testVerticalOffDemotesOnlyVertical(): void {
  assertEquals(
    resolveTabbarStyleFromNative(false, "vertical"),
    "horizontal",
    "vertical is demoted to horizontal",
  );
  assertEquals(
    resolveTabbarStyleFromNative(false, "horizontal"),
    "horizontal",
    "horizontal stays horizontal",
  );
  assertEquals(
    resolveTabbarStyleFromNative(false, "multirow"),
    "multirow",
    "multirow must be preserved",
  );
}

function testNormalizeTabbarStyle(): void {
  assertEquals(normalizeTabbarStyle("horizontal"), "horizontal", "horizontal");
  assertEquals(normalizeTabbarStyle("vertical"), "vertical", "vertical");
  assertEquals(normalizeTabbarStyle("multirow"), "multirow", "multirow");
  assertEquals(normalizeTabbarStyle("circular"), "horizontal", "unknown value");
  assertEquals(normalizeTabbarStyle(undefined), "horizontal", "undefined");
  assertEquals(normalizeTabbarStyle(null), "horizontal", "null");
  assertEquals(normalizeTabbarStyle(42), "horizontal", "number");
}

export async function runAllTests(): Promise<void> {
  await runTests("tabbar-style-sync.test.ts", [
    {
      name: "native vertical tabs on resolves to vertical",
      fn: testVerticalOnAlwaysVertical,
    },
    {
      name: "native vertical tabs off only demotes vertical",
      fn: testVerticalOffDemotesOnlyVertical,
    },
    {
      name: "unknown stored styles fall back to horizontal",
      fn: testNormalizeTabbarStyle,
    },
  ]);
}
