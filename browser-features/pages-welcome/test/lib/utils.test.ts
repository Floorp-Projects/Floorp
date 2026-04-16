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
    name: "basic merge",
    fn: () =>
      assertEquals(
        cn("text-sm", "font-bold"),
        "text-sm font-bold",
        "basic merge",
      ),
  },
  {
    name: "tailwind conflict and null filtering",
    fn: () =>
      assertEquals(
        cn("m-2", null, undefined, "m-4"),
        "m-4",
        "tailwind conflict and null filtering",
      ),
  },
  {
    name: "object conditional merge",
    fn: () =>
      assertEquals(
        cn("hidden", { block: false, flex: true }),
        "flex",
        "object conditional merge",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("utils.test.ts", tests);
}
