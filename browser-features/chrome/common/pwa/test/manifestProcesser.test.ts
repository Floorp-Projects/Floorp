// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ManifestProcesser } from "../manifestProcesser.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// Access private scopeIncludes via a test helper.
// Since scopeIncludes is private, we call it through the prototype with
// bracket notation to avoid TS access errors.
function scopeIncludes(scope: nsIURI, uri: nsIURI): boolean {
  const instance = new ManifestProcesser();
  // biome-ignore lint/suspicious/noExplicitAny: accessing private method for testing
  return (instance as any).scopeIncludes(scope, uri);
}

function makeURI(spec: string): nsIURI {
  return Services.io.newURI(spec);
}

const tests: TestCase[] = [
  // --- scopeIncludes ---
  {
    name: "scopeIncludes: exact match returns true",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("https://example.com/app/");
      assertEquals(scopeIncludes(scope, uri), true, "exact scope should match");
    },
  },
  {
    name: "scopeIncludes: URI within scope returns true",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("https://example.com/app/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        true,
        "page within scope should match",
      );
    },
  },
  {
    name: "scopeIncludes: nested path within scope returns true",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("https://example.com/app/sub/deep/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        true,
        "deeply nested path should match",
      );
    },
  },
  {
    name: "scopeIncludes: URI outside scope returns false",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("https://example.com/other/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        false,
        "different path should not match",
      );
    },
  },
  {
    name: "scopeIncludes: different origin returns false",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("https://other.com/app/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        false,
        "different origin should not match",
      );
    },
  },
  {
    name: "scopeIncludes: different scheme returns false",
    fn() {
      const scope = makeURI("https://example.com/app/");
      const uri = makeURI("http://example.com/app/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        false,
        "http vs https should not match",
      );
    },
  },
  {
    name: "scopeIncludes: different port returns false",
    fn() {
      const scope = makeURI("https://example.com:443/app/");
      const uri = makeURI("https://example.com:8080/app/page.html");
      assertEquals(
        scopeIncludes(scope, uri),
        false,
        "different port should not match",
      );
    },
  },
  {
    name: "scopeIncludes: root scope matches all paths on same origin",
    fn() {
      const scope = makeURI("https://example.com/");
      const uri = makeURI("https://example.com/any/path/here");
      assertEquals(
        scopeIncludes(scope, uri),
        true,
        "root scope should match all paths",
      );
    },
  },
  {
    name: "scopeIncludes: scope without trailing slash still works via filePath prefix",
    fn() {
      const scope = makeURI("https://example.com/app");
      const uri = makeURI("https://example.com/app/page.html");
      // /app is a prefix of /app/page.html via startsWith
      assertEquals(
        scopeIncludes(scope, uri),
        true,
        "scope without trailing slash should match sub-paths",
      );
    },
  },

  // --- ManifestProcesser instantiation ---
  {
    name: "ManifestProcesser can be instantiated",
    fn() {
      const mp = new ManifestProcesser();
      assert(mp !== null, "should create instance");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("manifestProcesser.test.ts", tests);
}
