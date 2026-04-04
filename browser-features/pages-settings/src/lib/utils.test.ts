// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("flex", "flex-col"), "flex flex-col", "basic flex classes");
  assertEquals(cn("p-1", "p-2", "p-3"), "p-3", "last spacing class should win");
  assertEquals(cn("shadow", undefined, "rounded"), "shadow rounded", "undefined should be ignored");
}
