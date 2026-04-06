// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "../../src/lib/utils.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "merge should preserve order",
    fn: () =>
      assertEquals(
        cn("inline", "items-center"),
        "inline items-center",
        "merge should preserve order",
      ),
  },
  {
    name: "array input",
    fn: () =>
      assertEquals(
        cn("rounded", ["border", "border-slate-200"]),
        "rounded border border-slate-200",
        "array input",
      ),
  },
  {
    name: "latest width should win",
    fn: () =>
      assertEquals(cn("w-10", "w-20"), "w-20", "latest width should win"),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
