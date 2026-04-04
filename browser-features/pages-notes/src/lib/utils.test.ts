// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("grid", "gap-2"), "grid gap-2", "grid classes");
  assertEquals(cn("text-xs", "text-sm"), "text-sm", "text size conflict should resolve");
  assertEquals(cn("bg-white", { "bg-slate-900": false }), "bg-white", "false object entries ignored");
}
