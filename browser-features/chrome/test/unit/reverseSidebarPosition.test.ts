// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { init } from "../../common/reverse-sidebar-position/index.ts";

import { assert, runTests, type TestCase } from "../utils/test_harness.ts";

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
  await runTests("reverseSidebarPosition.test.ts", [
    { name: "init function exists", fn: testInitFunctionExists },
    { name: "init does not throw", fn: testInitDoesNotThrow },
    { name: "init returns undefined", fn: testInitReturnsUndefined },
  ]);
}
