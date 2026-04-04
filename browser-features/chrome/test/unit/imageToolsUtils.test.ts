// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { isSvgMimeType } from "../../common/pwa/imageTools.ts";

type TestCase = {
  name: string;
  fn: () => void;
};

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — isSvgMimeType
// ---------------------------------------------------------------------------

function testSvgXml(): void {
  assert(isSvgMimeType("image/svg+xml"), '"image/svg+xml" should be SVG');
}

function testSvgXmlUppercase(): void {
  assert(isSvgMimeType("image/SVG+xml"), "case-insensitive should work");
}

function testSvgPlain(): void {
  assert(isSvgMimeType("image/svg"), '"image/svg" should be SVG');
}

function testPng(): void {
  assertEquals(
    isSvgMimeType("image/png"),
    false,
    '"image/png" should not be SVG',
  );
}

function testJpeg(): void {
  assertEquals(
    isSvgMimeType("image/jpeg"),
    false,
    '"image/jpeg" should not be SVG',
  );
}

function testNull(): void {
  assertEquals(isSvgMimeType(null), false, "null should not be SVG");
}

function testUndefined(): void {
  assertEquals(isSvgMimeType(undefined), false, "undefined should not be SVG");
}

function testEmptyString(): void {
  assertEquals(isSvgMimeType(""), false, "empty string should not be SVG");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "image/svg+xml", fn: testSvgXml },
    { name: "image/SVG+xml (case)", fn: testSvgXmlUppercase },
    { name: "image/svg", fn: testSvgPlain },
    { name: "image/png", fn: testPng },
    { name: "image/jpeg", fn: testJpeg },
    { name: "null", fn: testNull },
    { name: "undefined", fn: testUndefined },
    { name: "empty string", fn: testEmptyString },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `imageToolsUtils.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
