// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  formatFingerprintComment,
  formatSelectorMapEntry,
  parseFingerprintsFromMarkdown,
  parseSelectorMap,
} from "./fingerprint.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function testFingerprintCommentRoundTrip(): void {
  const comment = formatFingerprintComment({
    short: "abc12345",
    full: "abc12345def67890",
    path: "html/body/p",
  });

  assertEquals(comment, "<!--fp:abc12345-->", "comment format");

  const parsed = parseFingerprintsFromMarkdown(`before ${comment} after`);
  assertEquals(parsed.length, 1, "single fingerprint should be parsed");
  assertEquals(parsed[0].fingerprint, "abc12345", "fingerprint value");
  assert(
    parsed[0].endIndex > parsed[0].startIndex,
    "fingerprint index range should be valid",
  );
}

function testSelectorMapRoundTrip(): void {
  const entry = formatSelectorMapEntry(
    {
      short: "abc12345",
      full: "abc12345def67890",
      path: "html/body/div",
    },
    "div",
    'line1\nline2 | quoted "text"',
  );

  const markdown = [
    "# Selector Map",
    entry,
    'fp:ffffffffffffffff | span | "another entry"',
  ].join("\n");

  const parsed = parseSelectorMap(markdown);
  assertEquals(parsed.length, 2, "two selector map entries");
  assertEquals(parsed[0].fingerprint, "abc12345def67890", "first fingerprint");
  assertEquals(parsed[0].tagName, "div", "first tag name");
  assert(
    parsed[0].textPreview.includes('line1 line2 | quoted "text"'),
    "escaped preview should be decoded",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "fingerprint comment round trip",
      fn: testFingerprintCommentRoundTrip,
    },
    { name: "selector map round trip", fn: testSelectorMapRoundTrip },
  ];
  await runTests("fingerprintParsing.test.ts", tests);
}
