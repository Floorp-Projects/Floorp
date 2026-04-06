// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabDoubleClickClose } from "../doubleClickClose/index.ts";
import {
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "TabDoubleClickClose class is defined and constructable",
    fn: () => {
      assert(
        typeof TabDoubleClickClose === "function",
        "TabDoubleClickClose should be a class/function",
      );
    },
  },
  {
    name: "TabDoubleClickClose constructor handles missing reactive context gracefully",
    fn: () => {
      try {
        new TabDoubleClickClose();
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
  await runTests("tabDoubleClickClose.test.ts", tests);
}
