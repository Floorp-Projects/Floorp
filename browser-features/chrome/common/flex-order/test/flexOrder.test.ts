// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { gFlexOrder } from "../flex-order.tsx";

import {
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testGFlexOrderExports(): void {
  assert(
    typeof gFlexOrder.init === "function",
    "gFlexOrder.init should be a function",
  );
  assert(
    typeof gFlexOrder.applyFlexOrder === "function",
    "gFlexOrder.applyFlexOrder should be a function",
  );
}

function testApplyFlexOrderDoesNotThrow(): void {
  // applyFlexOrder updates internal signals — should not throw for any combination
  gFlexOrder.applyFlexOrder(true, true);
  gFlexOrder.applyFlexOrder(true, false);
  gFlexOrder.applyFlexOrder(false, true);
  gFlexOrder.applyFlexOrder(false, false);
}

function testSidebarPositionPrefExists(): void {
  const prefValue = Services.prefs.getBoolPref("sidebar.position_start", true);
  assert(
    typeof prefValue === "boolean",
    "sidebar.position_start pref should return a boolean",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("flexOrder.test.ts", [
    { name: "gFlexOrder exports", fn: testGFlexOrderExports },
    {
      name: "applyFlexOrder does not throw",
      fn: testApplyFlexOrderDoesNotThrow,
    },
    { name: "sidebar.position_start pref", fn: testSidebarPositionPrefExists },
  ]);
}
