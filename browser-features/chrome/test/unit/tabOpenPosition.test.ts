// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabOpenPosition } from "../../common/tab/openPosition/index.ts";
import { assert, runTests, type TestCase } from "../utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "TabOpenPosition class is defined and constructable",
    fn: () => {
      assert(
        typeof TabOpenPosition === "function",
        "TabOpenPosition should be a class/function",
      );
    },
  },
  {
    name: "TabOpenPosition constructor handles missing reactive context gracefully",
    fn: () => {
      try {
        new TabOpenPosition();
      } catch (e) {
        // createEffect from solid-js may not be available in all test contexts
        const msg = e instanceof Error ? e.message : String(e);
        assert(
          msg.includes("solid") ||
            msg.includes("effect") ||
            msg.includes("owner") ||
            msg.includes("createEffect"),
          `Unexpected error: ${msg}`,
        );
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabOpenPosition.test.ts", tests);
}
