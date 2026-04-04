// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("h-4", "w-4"), "h-4 w-4", "size classes merge");
  assertEquals(cn("w-4", "w-8"), "w-8", "width conflict resolves to latest");
  assertEquals(cn("opacity-50", ["transition", "duration-200"]), "opacity-50 transition duration-200", "array merge");
}
