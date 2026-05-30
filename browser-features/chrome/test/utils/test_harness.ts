// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * Shared test harness for browser-side unit tests.
 *
 * Provides assertion utilities, test case types, and a standard runner
 * that collects all failures before throwing.
 *
 * Usage:
 * ```typescript
 * import { assertEquals, runTests, type TestCase } from "../utils/test_harness.ts";
 *
 * function testSomething(): void {
 *   assertEquals(1 + 1, 2, "basic addition");
 * }
 *
 * export async function runAllTests(): Promise<void> {
 *   await runTests("myModule.test.ts", [
 *     { name: "something works", fn: testSomething },
 *   ]);
 * }
 * ```
 */

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type TestCase = {
  name: string;
  fn: () => void | Promise<void>;
};

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------

export function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

export function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

export function assertNotEquals<T>(
  actual: T,
  expected: T,
  message: string,
): void {
  if (actual === expected) {
    throw new Error(
      `${message}: expected values to differ but both were ${String(expected)}`,
    );
  }
}

export function assertThrows(fn: () => unknown, message: string): Error | null {
  try {
    fn();
    throw new Error(`${message}: expected function to throw but it did not`);
  } catch (e: unknown) {
    if (
      e instanceof Error &&
      e.message.includes("expected function to throw")
    ) {
      throw e;
    }
    return e instanceof Error ? e : null;
  }
}

/**
 * Assert that the given function does not throw.
 */
export function assertDoesNotThrow(
  fn: () => void,
  message?: string,
): void {
  try {
    fn();
  } catch (err) {
    throw new Error(
      message ?? `Expected no throw, but got: ${String(err)}`,
    );
  }
}

/**
 * Assert that `actual` is approximately equal to `expected` within `tolerance`.
 */
export function assertApprox(
  actual: number,
  expected: number,
  tolerance: number,
  message: string,
): void {
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error(
      `${message} (expected: ~${expected} ± ${tolerance}, actual: ${actual})`,
    );
  }
}

// ---------------------------------------------------------------------------
// Test Runner (Pattern A: collect all failures, throw at end)
// ---------------------------------------------------------------------------

/**
 * Run an array of test cases, collect all failures, and throw a combined
 * error at the end if any test failed.
 *
 * @param moduleName - Name of the test file (for error messages)
 * @param tests - Array of test cases to run sequentially
 */
export async function runTests(
  moduleName: string,
  tests: TestCase[],
): Promise<void> {
  const failures: string[] = [];

  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
  }
}
