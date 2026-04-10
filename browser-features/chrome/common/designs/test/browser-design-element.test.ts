// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { replaceIconPaths } from "../browser-design-element.tsx";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — replaceIconPaths
// ---------------------------------------------------------------------------

function testReplaceIconPathsBasic(): void {
  const css = ".icon { background: url(../icons/foo.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assertEquals(
    result,
    ".icon { background: url(https://example.com/icons/foo.svg); }",
    "relative ../icons should be replaced with iconBasePath",
  );
}

function testReplaceIconPathsMultiple(): void {
  const css =
    ".a { background: url(../icons/a.svg); } .b { background: url(../icons/b.svg); }";
  const result = replaceIconPaths(css, "http://localhost:5174/icons");
  assert(
    !result.includes("../icons"),
    "no ../icons should remain after replacement",
  );
  assert(
    result.includes("http://localhost:5174/icons/a.svg"),
    "first icon path should be replaced",
  );
  assert(
    result.includes("http://localhost:5174/icons/b.svg"),
    "second icon path should be replaced",
  );
}

function testReplaceIconPathsUndefinedBasePath(): void {
  const css = ".icon { background: url(../icons/foo.svg); }";
  const result = replaceIconPaths(css, undefined);
  assertEquals(
    result,
    css,
    "undefined iconBasePath should return CSS unchanged",
  );
}

function testReplaceIconPathsNoIcons(): void {
  const css = ".foo { color: red; }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assertEquals(
    result,
    ".foo { color: red; }",
    "CSS without ../icons should be unchanged",
  );
}

function testReplaceIconPathsEmptyString(): void {
  const result = replaceIconPaths("", "https://example.com/icons");
  assertEquals(result, "", "empty CSS should return empty string");
}

function testReplaceIconPathsOnlyReplacesRelativeIcons(): void {
  const css =
    ".icon { background: url(../icons/foo.svg); } .other { background: url(chrome://floorp/skin/bar.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assert(
    result.includes("chrome://floorp/skin/bar.svg"),
    "non-relative paths should not be modified",
  );
  assert(!result.includes("../icons"), "relative ../icons should be replaced");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("browser-design-element.test.ts", [
    { name: "replaceIconPaths basic", fn: testReplaceIconPathsBasic },
    { name: "replaceIconPaths multiple", fn: testReplaceIconPathsMultiple },
    {
      name: "replaceIconPaths undefined basePath",
      fn: testReplaceIconPathsUndefinedBasePath,
    },
    { name: "replaceIconPaths no icons", fn: testReplaceIconPathsNoIcons },
    {
      name: "replaceIconPaths empty string",
      fn: testReplaceIconPathsEmptyString,
    },
    {
      name: "replaceIconPaths only replaces relative",
      fn: testReplaceIconPathsOnlyReplacesRelativeIcons,
    },
    {
      name: "replaceIconPaths with trailing slash",
      fn: testReplaceIconPathsTrailingSlash,
    },
    {
      name: "replaceIconPaths with port number",
      fn: testReplaceIconPathsWithPortNumber,
    },
    {
      name: "replaceIconPaths special characters in path",
      fn: testReplaceIconPathsSpecialCharacters,
    },
    {
      name: "replaceIconPaths multiple consecutive",
      fn: testReplaceIconPathsMultipleConsecutive,
    },
    {
      name: "replaceIconPaths case sensitive",
      fn: testReplaceIconPathsCaseSensitive,
    },
    {
      name: "replaceIconPaths null basePath",
      fn: testReplaceIconPathsNullBasePath,
    },
  ]);
}

// ---------------------------------------------------------------------------
// Additional Tests — replaceIconPaths edge cases
// ---------------------------------------------------------------------------

function testReplaceIconPathsTrailingSlash(): void {
  const css = ".icon { background: url(../icons/foo.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons/");
  assert(
    result.includes("https://example.com/icons//foo.svg"),
    "trailing slash should result in double slash (preserves user input)",
  );
}

function testReplaceIconPathsWithPortNumber(): void {
  const css = ".icon { background: url(../icons/foo.svg); }";
  const result = replaceIconPaths(css, "http://localhost:5173/icons");
  assert(
    result.includes("http://localhost:5173/icons/foo.svg"),
    "port number should be preserved",
  );
}

function testReplaceIconPathsSpecialCharacters(): void {
  const css = ".icon { background: url(../icons/icon-with-dash.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assert(
    result.includes("https://example.com/icons/icon-with-dash.svg"),
    "special characters in filename should be preserved",
  );
}

function testReplaceIconPathsMultipleConsecutive(): void {
  const css = ".icon { background: url(../icons/a.svg); url(../icons/b.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assert(
    result.includes("https://example.com/icons/a.svg"),
    "first icon should be replaced",
  );
  assert(
    result.includes("https://example.com/icons/b.svg"),
    "second icon should be replaced",
  );
  assert(
    !result.includes("../icons"),
    "no ../icons should remain",
  );
}

function testReplaceIconPathsCaseSensitive(): void {
  const css = ".icon { background: url(../ICONS/foo.svg); }";
  const result = replaceIconPaths(css, "https://example.com/icons");
  assert(
    result.includes("../ICONS"),
    "uppercase ICONS should not be replaced (case sensitive)",
  );
}

function testReplaceIconPathsNullBasePath(): void {
  const css = ".icon { background: url(../icons/foo.svg); }";
  const result = replaceIconPaths(css, null as unknown as string);
  assertEquals(
    result,
    css,
    "null basePath should return CSS unchanged",
  );
}
