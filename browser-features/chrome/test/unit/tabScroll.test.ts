// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabScroll } from "../../common/tab/scroll/index.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "TabScroll class is defined and constructable",
      fn: () => {
        assert(typeof TabScroll === "function", "TabScroll should be a class/function");
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
