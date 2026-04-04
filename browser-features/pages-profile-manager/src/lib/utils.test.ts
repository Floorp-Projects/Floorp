// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  assertEquals(cn("inline", "items-center"), "inline items-center", "merge should preserve order");
  assertEquals(cn("rounded", ["border", "border-slate-200"]), "rounded border border-slate-200", "array input");
  assertEquals(cn("w-10", "w-20"), "w-20", "latest width should win");
}
