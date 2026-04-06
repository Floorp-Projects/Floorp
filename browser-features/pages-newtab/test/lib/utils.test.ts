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
    name: "simple class merge",
    fn: () =>
      assertEquals(cn("px-2", "py-1"), "px-2 py-1", "simple class merge"),
  },
  {
    name: "falsy values should be ignored",
    fn: () =>
      assertEquals(
        cn("px-2", false && "hidden", "py-1"),
        "px-2 py-1",
        "falsy values should be ignored",
      ),
  },
  {
    name: "tailwind conflict should prefer later class",
    fn: () =>
      assertEquals(
        cn("p-2", "p-4"),
        "p-4",
        "tailwind conflict should prefer later class",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
