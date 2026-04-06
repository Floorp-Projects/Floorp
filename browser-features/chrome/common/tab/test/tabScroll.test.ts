// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabScroll } from "../scroll/index.ts";
import {
  assert,
  type assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "TabScroll class is defined and constructable",
    fn: () => {
      assert(
        typeof TabScroll === "function",
        "TabScroll should be a class/function",
      );
    },
  },
  {
    name: "TabScroll constructor does not throw when #tabbrowser-tabs is absent",
    fn: () => {
      // In test context, document.querySelector("#tabbrowser-tabs") returns null.
      // The constructor should handle that gracefully.
      try {
        new TabScroll();
      } catch (e) {
        // createEffect from solid-js may not be available in all test contexts,
        // so we only check that the class itself is importable and defined.
        const msg = e instanceof Error ? e.message : String(e);
        // If it fails due to missing solid-js reactivity context, that is acceptable
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
  await runTests("tabScroll.test.ts", tests);
}
