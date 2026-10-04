// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { toExportFilename } from "../../src/lib/exportFilename.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function testSimpleTitle(): void {
  assertEquals(
    toExportFilename("My Note", "Untitled"),
    "My Note.md",
    "a clean title passes through, spaces included",
  );
}

function testForbiddenCharacters(): void {
  assertEquals(
    toExportFilename('a/b\\c:d*e?f"g<h>i|j', "Untitled"),
    "a-b-c-d-e-f-g-h-i-j.md",
    "forbidden characters become hyphens",
  );
}

function testControlCharactersAndWhitespace(): void {
  assertEquals(
    toExportFilename("a\nb\tc   d", "Untitled"),
    "a b c d.md",
    "control characters and runs of whitespace collapse to one space",
  );
}

function testBidiOverrideIsNeutralised(): void {
  assertEquals(
    toExportFilename("a\u202Egnp.md", "Untitled"),
    "a gnp.md.md",
    "a right-to-left override cannot be used to disguise the extension",
  );
}

function testEmptyTitleUsesFallback(): void {
  assertEquals(
    toExportFilename("   ", "Untitled"),
    "Untitled.md",
    "blank title uses the fallback",
  );
  assertEquals(
    toExportFilename("///", "Untitled"),
    "Untitled.md",
    "title that sanitises to nothing uses the fallback",
  );
}

function testTrailingDotsAndSpaces(): void {
  assertEquals(
    toExportFilename("report...", "Untitled"),
    "report.md",
    "trailing dots are stripped",
  );
}

function testReservedDeviceName(): void {
  assertEquals(
    toExportFilename("CON", "Untitled"),
    "CON_.md",
    "reserved Windows device names get a suffix",
  );
  assertEquals(
    toExportFilename("com1", "Untitled"),
    "com1_.md",
    "reserved names are matched case-insensitively",
  );
  assertEquals(
    toExportFilename("CON.txt", "Untitled"),
    "CON.txt_.md",
    "Windows resolves device names up to the first dot",
  );
}

function testLengthCap(): void {
  const long = "x".repeat(200);
  const result = toExportFilename(long, "Untitled");
  assertEquals(result.length, 103, "stem is capped at 100 characters plus '.md'");
}

function testTruncationDoesNotSplitSurrogatePairs(): void {
  const result = toExportFilename("a".repeat(99) + "\u{1F600}", "Untitled");
  assertEquals(
    /[\uD800-\uDBFF](?![\uDC00-\uDFFF])/.test(result),
    false,
    "truncation must not leave a lone surrogate in the filename",
  );
}

function testCustomExtension(): void {
  assertEquals(
    toExportFilename("notes", "Untitled", "txt"),
    "notes.txt",
    "extension is configurable",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "simple title", fn: testSimpleTitle },
    { name: "forbidden characters", fn: testForbiddenCharacters },
    { name: "control characters and whitespace", fn: testControlCharactersAndWhitespace },
    { name: "bidi override", fn: testBidiOverrideIsNeutralised },
    { name: "empty title uses fallback", fn: testEmptyTitleUsesFallback },
    { name: "trailing dots and spaces", fn: testTrailingDotsAndSpaces },
    { name: "reserved device name", fn: testReservedDeviceName },
    { name: "length cap", fn: testLengthCap },
    { name: "surrogate-safe truncation", fn: testTruncationDoesNotSplitSurrogatePairs },
    { name: "custom extension", fn: testCustomExtension },
  ];

  await runTests("exportFilename.test.ts", tests);
}
