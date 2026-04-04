// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { init } from "../../common/reverse-sidebar-position/index.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function testInitFunctionExists(): void {
  assert(typeof init === "function", "init should be an exported function");
}

function testInitDoesNotThrow(): void {
  // The init function currently has its body commented out (no-op),
  // so calling it should not throw.
  let threw = false;
  try {
    init();
  } catch {
    threw = true;
  }
  assert(!threw, "init() should not throw");
}

function testInitReturnsUndefined(): void {
  const result = init();
  assert(result === undefined, "init() should return undefined");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "init function exists", fn: testInitFunctionExists },
    { name: "init does not throw", fn: testInitDoesNotThrow },
    { name: "init returns undefined", fn: testInitReturnsUndefined },
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
    throw new Error(`reverseSidebarPosition.test.ts failures: ${failures.join(" | ")}`);
}
