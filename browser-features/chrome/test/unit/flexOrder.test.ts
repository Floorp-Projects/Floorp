// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { gFlexOrder } from "../../common/flex-order/flex-order.tsx";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

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
  const tests: TestCase[] = [
    { name: "gFlexOrder exports", fn: testGFlexOrderExports },
    { name: "applyFlexOrder does not throw", fn: testApplyFlexOrderDoesNotThrow },
    { name: "sidebar.position_start pref", fn: testSidebarPositionPrefExists },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }
  if (failures.length > 0)
    throw new Error(`flexOrder.test.ts failures: ${failures.join(" | ")}`);
}
