// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getChromeWebStoreInstallInfo,
  clearChromeWebStoreInstallInfo,
} from "../observer.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testGetInstallInfoInitiallyNull(): void {
  // Clear first to ensure clean state
  clearChromeWebStoreInstallInfo();
  const info = getChromeWebStoreInstallInfo();
  assertEquals(info, null, "should be null when no pending install");
}

function testClearInstallInfoNoOp(): void {
  // Clearing when nothing is set should not throw
  clearChromeWebStoreInstallInfo();
  clearChromeWebStoreInstallInfo();
  const info = getChromeWebStoreInstallInfo();
  assertEquals(info, null, "should remain null after multiple clears");
}

function testGetInstallInfoReturnType(): void {
  const info = getChromeWebStoreInstallInfo();
  assert(
    info === null || typeof info === "object",
    "should be null or an object",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "initial install info is null",
      fn: testGetInstallInfoInitiallyNull,
    },
    { name: "clear install info no-op", fn: testClearInstallInfoNoOp },
    { name: "get install info return type", fn: testGetInstallInfoReturnType },
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
    throw new Error(`addonsObserver.test.ts failures: ${failures.join(" | ")}`);
  }
}
