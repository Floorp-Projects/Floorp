// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { estimateBytesFromDataURL } from "../../src/utils/imageCompression.ts";
import {
  assertEquals,
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — estimateBytesFromDataURL
// ---------------------------------------------------------------------------

function testEmptyDataUrl(): void {
  assertEquals(
    estimateBytesFromDataURL("data:image/png;base64,"),
    0,
    "empty base64 should be 0 bytes",
  );
}

function testNoPart(): void {
  assertEquals(
    estimateBytesFromDataURL("data:image/png"),
    0,
    "no comma separator should return 0",
  );
}

function testKnownSize(): void {
  // "AAAA" in base64 = 3 bytes
  const result = estimateBytesFromDataURL("data:image/png;base64,AAAA");
  assertEquals(result, 3, '"AAAA" should decode to 3 bytes');
}

function testWithPadding(): void {
  // "AA==" in base64 = 1 byte (4 chars, 2 padding)
  const result = estimateBytesFromDataURL("data:image/png;base64,AA==");
  assertEquals(result, 1, '"AA==" should decode to 1 byte');
}

function testWithSinglePadding(): void {
  // "AAA=" in base64 = 2 bytes (4 chars, 1 padding)
  const result = estimateBytesFromDataURL("data:image/png;base64,AAA=");
  assertEquals(result, 2, '"AAA=" should decode to 2 bytes');
}

function testLargerPayload(): void {
  // 8 base64 chars, no padding = 6 bytes
  const result = estimateBytesFromDataURL("data:image/png;base64,AAAAAAAA");
  assertEquals(result, 6, "8 base64 chars should decode to 6 bytes");
}

function testPositiveResult(): void {
  // Any reasonable data URL should produce a positive result
  const realish =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
  const result = estimateBytesFromDataURL(realish);
  assert(result > 0, "real PNG data URL should have positive byte count");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "empty data URL", fn: testEmptyDataUrl },
    { name: "no comma part", fn: testNoPart },
    { name: "known size AAAA", fn: testKnownSize },
    { name: "with double padding", fn: testWithPadding },
    { name: "with single padding", fn: testWithSinglePadding },
    { name: "larger payload", fn: testLargerPayload },
    { name: "positive result for real PNG", fn: testPositiveResult },
  ];

  await runTests("imageCompression.test.ts", tests);
}
