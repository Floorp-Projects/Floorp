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
    name: "size classes merge",
    fn: () => assertEquals(cn("h-4", "w-4"), "h-4 w-4", "size classes merge"),
  },
  {
    name: "width conflict resolves to latest",
    fn: () =>
      assertEquals(
        cn("w-4", "w-8"),
        "w-8",
        "width conflict resolves to latest",
      ),
  },
  {
    name: "array merge",
    fn: () =>
      assertEquals(
        cn("opacity-50", ["transition", "duration-200"]),
        "opacity-50 transition duration-200",
        "array merge",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
