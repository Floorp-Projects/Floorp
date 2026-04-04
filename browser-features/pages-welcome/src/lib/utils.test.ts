// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("text-sm", "font-bold"), "text-sm font-bold", "basic merge");
  assertEquals(cn("m-2", null, undefined, "m-4"), "m-4", "tailwind conflict and null filtering");
  assertEquals(cn("hidden", { block: false, flex: true }), "flex", "object conditional merge");
}
