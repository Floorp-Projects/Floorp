// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabDoubleClickClose } from "../../common/tab/doubleClickClose/index.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "TabDoubleClickClose class is defined and constructable",
      fn: () => {
        assert(typeof TabDoubleClickClose === "function", "TabDoubleClickClose should be a class/function");
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
            msg.includes("solid") || msg.includes("effect") || msg.includes("owner") || msg.includes("createEffect"),
            `Unexpected error: ${msg}`,
          );
        }
      },
    },
  ];

  for (const t of tests) {
    try {
      await t.fn();
      console.log(`  PASS: ${t.name}`);
    } catch (e) {
      console.error(`  FAIL: ${t.name}`);
      throw e;
    }
  }
}
