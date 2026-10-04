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

type TestProgress = {
  moduleName: string;
  testName: string;
  status: "running" | "passed" | "failed" | "done";
  index: number;
  total: number;
  startedAtMs: number;
};

declare global {
  var __NORA_TEST_PROGRESS__: TestProgress | undefined;
}

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

  for (const [index, test] of tests.entries()) {
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: test.name,
      status: "running",
      index: index + 1,
      total: tests.length,
      startedAtMs: Date.now(),
    };

    try {
      await test.fn();
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ??
        Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "passed",
        index: index + 1,
        total: tests.length,
        startedAtMs,
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ??
        Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "failed",
        index: index + 1,
        total: tests.length,
        startedAtMs,
      };
    }
  }

  globalThis.__NORA_TEST_PROGRESS__ = {
    moduleName,
    testName: "",
    status: "done",
    index: tests.length,
    total: tests.length,
    startedAtMs: Date.now(),
  };

  if (failures.length > 0) {
    throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
  }
}
