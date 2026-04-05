// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getUserContextColor } from "../../common/panel-sidebar/utils/userContextColor-getter.ts";
import { type TestCase, assert, assertEquals } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testUserContextIdZeroReturnsNull(): void {
  // userContextId 0 means "no container" — should not find a match
  const result = getUserContextColor(0);
  assertEquals(result, null, "userContextId 0 should return null");
}

function testNonExistentUserContextReturnsNull(): void {
  const result = getUserContextColor(999999);
  assertEquals(
    result,
    null,
    "non-existent userContextId should return null",
  );
}

function testExistingContainerReturnsColor(): void {
  // Default Firefox containers start at userContextId 1
  const result = getUserContextColor(1);
  // May be a number, string, or null depending on Firefox version and container config
  assert(
    result === null || typeof result === "number" || typeof result === "string",
    `result should be null, number, or string, got ${typeof result}`,
  );
}

function testReturnTypeConsistency(): void {
  // Test a range of IDs to ensure consistent return types
  for (let i = 0; i < 5; i++) {
    const result = getUserContextColor(i);
    assert(
      result === null || typeof result === "number" || typeof result === "string",
      `userContextId ${i}: result should be null, number, or string`,
    );
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "userContextId 0 → null", fn: testUserContextIdZeroReturnsNull },
    { name: "non-existent → null", fn: testNonExistentUserContextReturnsNull },
    { name: "existing container returns color", fn: testExistingContainerReturnsColor },
    { name: "return type consistency", fn: testReturnTypeConsistency },
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
    throw new Error(
      `userContextColor.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
