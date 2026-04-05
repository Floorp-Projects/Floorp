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
    name: "basic flex classes",
    fn: () =>
      assertEquals(
        cn("flex", "flex-col"),
        "flex flex-col",
        "basic flex classes",
      ),
  },
  {
    name: "last spacing class should win",
    fn: () =>
      assertEquals(
        cn("p-1", "p-2", "p-3"),
        "p-3",
        "last spacing class should win",
      ),
  },
  {
    name: "undefined should be ignored",
    fn: () =>
      assertEquals(
        cn("shadow", undefined, "rounded"),
        "shadow rounded",
        "undefined should be ignored",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
