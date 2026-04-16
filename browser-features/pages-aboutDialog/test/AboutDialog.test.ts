// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "Document should exist in browser test context",
    fn: () =>
      assert(
        typeof document !== "undefined",
        "Document should exist in browser test context",
      ),
  },
  {
    name: "Window should exist in browser test context",
    fn: () =>
      assert(
        typeof window !== "undefined",
        "Window should exist in browser test context",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("AboutDialog.test.ts", tests);
}
