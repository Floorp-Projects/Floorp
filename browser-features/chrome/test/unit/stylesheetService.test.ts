// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { StyleSheetServiceUtils } from "../../utils/stylesheet-service.ts";

type TestCase = { name: string; fn: () => void | Promise<void> };
function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}
function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

// Use a data: URI that is valid CSS so the style sheet service accepts it.
const TEST_CSS_URI = "data:text/css,*{}";
const TEST_CSS_URI_2 = "data:text/css,body{}";

const tests: TestCase[] = [
  {
    name: "checkStyleSheetLoaded returns false for unloaded sheet",
    fn() {
      // Use a URI unlikely to be registered already
      const uri = "data:text/css,.floorp-test-not-loaded{}";
      // Ensure clean state
      if (StyleSheetServiceUtils.checkStyleSheetLoaded(uri)) {
        StyleSheetServiceUtils.unloadStyleSheet(uri);
      }
      assertEquals(
        StyleSheetServiceUtils.checkStyleSheetLoaded(uri),
        false,
        "sheet should not be loaded initially",
      );
    },
  },
  {
    name: "loadStyleSheetWith registers a sheet",
    fn() {
      try {
        StyleSheetServiceUtils.loadStyleSheetWith(TEST_CSS_URI);
        assertEquals(
          StyleSheetServiceUtils.checkStyleSheetLoaded(TEST_CSS_URI),
          true,
          "sheet should be loaded after loadStyleSheetWith",
        );
      } finally {
        // Cleanup
        if (StyleSheetServiceUtils.checkStyleSheetLoaded(TEST_CSS_URI)) {
          StyleSheetServiceUtils.unloadStyleSheet(TEST_CSS_URI);
        }
      }
    },
  },
  {
    name: "unloadStyleSheet unregisters a loaded sheet",
    fn() {
      StyleSheetServiceUtils.loadStyleSheetWith(TEST_CSS_URI_2);
      assertEquals(
        StyleSheetServiceUtils.checkStyleSheetLoaded(TEST_CSS_URI_2),
        true,
        "sheet should be loaded",
      );

      StyleSheetServiceUtils.unloadStyleSheet(TEST_CSS_URI_2);
      assertEquals(
        StyleSheetServiceUtils.checkStyleSheetLoaded(TEST_CSS_URI_2),
        false,
        "sheet should be unloaded after unloadStyleSheet",
      );
    },
  },
  {
    name: "loadStyleSheetWith is idempotent (loading twice does not throw)",
    fn() {
      const uri = "data:text/css,.floorp-idempotent-test{}";
      try {
        StyleSheetServiceUtils.loadStyleSheetWith(uri);
        // Loading same sheet again should not throw
        StyleSheetServiceUtils.loadStyleSheetWith(uri);
        assertEquals(
          StyleSheetServiceUtils.checkStyleSheetLoaded(uri),
          true,
          "sheet should still be loaded",
        );
      } finally {
        if (StyleSheetServiceUtils.checkStyleSheetLoaded(uri)) {
          StyleSheetServiceUtils.unloadStyleSheet(uri);
        }
      }
    },
  },
  {
    name: "load, unload, reload cycle works",
    fn() {
      const uri = "data:text/css,.floorp-cycle-test{}";
      try {
        StyleSheetServiceUtils.loadStyleSheetWith(uri);
        assertEquals(
          StyleSheetServiceUtils.checkStyleSheetLoaded(uri),
          true,
          "loaded",
        );

        StyleSheetServiceUtils.unloadStyleSheet(uri);
        assertEquals(
          StyleSheetServiceUtils.checkStyleSheetLoaded(uri),
          false,
          "unloaded",
        );

        StyleSheetServiceUtils.loadStyleSheetWith(uri);
        assertEquals(
          StyleSheetServiceUtils.checkStyleSheetLoaded(uri),
          true,
          "reloaded",
        );
      } finally {
        if (StyleSheetServiceUtils.checkStyleSheetLoaded(uri)) {
          StyleSheetServiceUtils.unloadStyleSheet(uri);
        }
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  for (const t of tests) {
    try {
      await t.fn();
      console.log(`[PASS] ${t.name}`);
    } catch (e) {
      console.error(`[FAIL] ${t.name}:`, e);
      throw e;
    }
  }
}
