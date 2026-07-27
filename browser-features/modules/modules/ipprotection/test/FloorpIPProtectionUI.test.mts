// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { resolveFloorpIPProtectionDisclosureStrings } from "../FloorpIPProtectionDisclosure.sys.mts";
import {
  filterFloorpIPProtectionCallouts,
  resolveFloorpIPProtectionToolbarTooltip,
} from "../FloorpIPProtectionUI.sys.mts";

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function testFiltersOnlyIPProtectionCallouts(): void {
  const messages = [
    { id: "IP_PROTECTION_FIRST_RUN" },
    { id: "OTHER_FEATURE" },
    { id: "IP_PROTECTION_REMINDER" },
    { id: 42 },
  ];
  assertEquals(
    JSON.stringify(filterFloorpIPProtectionCallouts(messages)),
    JSON.stringify([{ id: "OTHER_FEATURE" }, { id: 42 }]),
    "IP Protection callouts should be removed without affecting other messages",
  );
}

function testToolbarTooltipPrecedence(): void {
  const strings = resolveFloorpIPProtectionDisclosureStrings("en-US");
  const cases: Array<[string[], string]> = [
    [[], strings.toolbarInactiveTooltip],
    [["ipprotection-on"], strings.toolbarActiveTooltip],
    [["ipprotection-excluded"], strings.toolbarExcludedTooltip],
    [["ipprotection-paused"], strings.toolbarPausedTooltip],
    [["ipprotection-error"], strings.toolbarErrorTooltip],
    [["ipprotection-network-error"], strings.toolbarErrorTooltip],
    [
      ["ipprotection-on", "ipprotection-paused"],
      strings.toolbarPausedTooltip,
    ],
    [
      ["ipprotection-on", "ipprotection-error"],
      strings.toolbarErrorTooltip,
    ],
  ];

  for (const [classes, expected] of cases) {
    assertEquals(
      resolveFloorpIPProtectionToolbarTooltip(classes, strings),
      expected,
      `toolbar classes ${
        classes.join(",") || "inactive"
      } should resolve correctly`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "filters only IP Protection callouts",
      fn: testFiltersOnlyIPProtectionCallouts,
    },
    {
      name: "resolves toolbar tooltip precedence",
      fn: testToolbarTooltipPrecedence,
    },
  ];
  await runTests("FloorpIPProtectionUI.test.mts", tests);
}
