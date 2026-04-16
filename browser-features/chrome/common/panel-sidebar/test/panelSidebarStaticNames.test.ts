// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { PanelSidebarStaticNames } from "../utils/panel-sidebar-static-names.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testPrefNamesMatchExpectedValues(): void {
  assertEquals(
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    "floorp.panelSidebar.data",
    "data pref name should stay stable",
  );
  assertEquals(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    "floorp.panelSidebar.config",
    "config pref name should stay stable",
  );
  assertEquals(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    "floorp.panelSidebar.enabled",
    "enabled pref name should stay stable",
  );
}

function testPrefNamesAreNonEmptyAndUnique(): void {
  const values = [
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
  ];

  for (const value of values) {
    assert(value.length > 0, "preference name should not be empty");
    assert(
      value.startsWith("floorp.panelSidebar."),
      `preference should use panelSidebar namespace: ${value}`,
    );
  }

  assertEquals(
    new Set(values).size,
    values.length,
    "preference names should be unique",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "preference names match expected values",
      fn: testPrefNamesMatchExpectedValues,
    },
    {
      name: "preference names are non-empty and unique",
      fn: testPrefNamesAreNonEmptyAndUnique,
    },
  ];

  await runTests("panelSidebarStaticNames.test.ts", tests);
}
