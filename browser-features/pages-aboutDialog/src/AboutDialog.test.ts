// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

export async function runAllTests(): Promise<void> {
  assert(typeof document !== "undefined", "Document should exist in browser test context");
  assert(typeof window !== "undefined", "Window should exist in browser test context");
}
