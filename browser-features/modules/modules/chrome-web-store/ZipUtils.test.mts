// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getEntryExtension,
  isJavaScriptEntry,
  isJSONEntry,
} from "./ZipUtils.sys.mts";

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

const tests: TestCase[] = [
  // --- getEntryExtension ---
  {
    name: "getEntryExtension returns extension for simple filename",
    fn() {
      assertEquals(getEntryExtension("file.js"), "js", "should return js");
    },
  },
  {
    name: "getEntryExtension returns extension in lowercase",
    fn() {
      assertEquals(getEntryExtension("file.JSON"), "json", "should lowercase");
    },
  },
  {
    name: "getEntryExtension returns empty string for no extension",
    fn() {
      assertEquals(getEntryExtension("Makefile"), "", "should return empty");
    },
  },
  {
    name: "getEntryExtension handles nested paths",
    fn() {
      assertEquals(
        getEntryExtension("path/to/script.mjs"),
        "mjs",
        "should return mjs for nested path",
      );
    },
  },
  {
    name: "getEntryExtension handles multiple dots",
    fn() {
      assertEquals(
        getEntryExtension("jquery.min.js"),
        "js",
        "should return last extension",
      );
    },
  },
  {
    name: "getEntryExtension handles dotfile",
    fn() {
      assertEquals(
        getEntryExtension(".gitignore"),
        "gitignore",
        "should return part after dot",
      );
    },
  },

  // --- isJavaScriptEntry ---
  {
    name: "isJavaScriptEntry returns true for .js",
    fn() {
      assertEquals(isJavaScriptEntry("app.js"), true, ".js is JS");
    },
  },
  {
    name: "isJavaScriptEntry returns true for .mjs",
    fn() {
      assertEquals(isJavaScriptEntry("module.mjs"), true, ".mjs is JS");
    },
  },
  {
    name: "isJavaScriptEntry returns true for .jsx",
    fn() {
      assertEquals(isJavaScriptEntry("component.jsx"), true, ".jsx is JS");
    },
  },
  {
    name: "isJavaScriptEntry returns false for .ts",
    fn() {
      assertEquals(isJavaScriptEntry("types.ts"), false, ".ts is not JS");
    },
  },
  {
    name: "isJavaScriptEntry returns false for .json",
    fn() {
      assertEquals(isJavaScriptEntry("data.json"), false, ".json is not JS");
    },
  },
  {
    name: "isJavaScriptEntry returns false for .css",
    fn() {
      assertEquals(isJavaScriptEntry("style.css"), false, ".css is not JS");
    },
  },
  {
    name: "isJavaScriptEntry handles nested paths",
    fn() {
      assertEquals(
        isJavaScriptEntry("src/lib/helper.js"),
        true,
        "nested .js is JS",
      );
    },
  },

  // --- isJSONEntry ---
  {
    name: "isJSONEntry returns true for .json",
    fn() {
      assertEquals(isJSONEntry("manifest.json"), true, ".json is JSON");
    },
  },
  {
    name: "isJSONEntry returns false for .js",
    fn() {
      assertEquals(isJSONEntry("script.js"), false, ".js is not JSON");
    },
  },
  {
    name: "isJSONEntry returns true for uppercase .JSON",
    fn() {
      assertEquals(isJSONEntry("DATA.JSON"), true, ".JSON should be JSON");
    },
  },
  {
    name: "isJSONEntry returns false for .jsonl",
    fn() {
      assertEquals(isJSONEntry("data.jsonl"), false, ".jsonl is not JSON");
    },
  },
  {
    name: "isJSONEntry handles nested paths",
    fn() {
      assertEquals(
        isJSONEntry("_locales/en/messages.json"),
        true,
        "nested .json is JSON",
      );
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
