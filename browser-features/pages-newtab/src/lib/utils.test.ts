// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("px-2", "py-1"), "px-2 py-1", "simple class merge");
  assertEquals(cn("px-2", false && "hidden", "py-1"), "px-2 py-1", "falsy values should be ignored");
  assertEquals(cn("p-2", "p-4"), "p-4", "tailwind conflict should prefer later class");
}
