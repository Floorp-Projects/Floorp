// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { cn } from "./utils.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "grid classes",
    fn: () => assertEquals(cn("grid", "gap-2"), "grid gap-2", "grid classes"),
  },
  {
    name: "text size conflict should resolve",
    fn: () =>
      assertEquals(
        cn("text-xs", "text-sm"),
        "text-sm",
        "text size conflict should resolve",
      ),
  },
  {
    name: "false object entries ignored",
    fn: () =>
      assertEquals(
        cn("bg-white", { "bg-slate-900": false }),
        "bg-white",
        "false object entries ignored",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
