// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { IconProcesser } from "../iconProcesser.ts";
import type { Icon } from "../type.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

function makeIcon(sizes: string[]): Icon {
  return { src: "https://example.com/icon.png", sizes };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testEmptyArray(): void {
  const result = IconProcesser.getInstance().buildIconList([]);
  assertEquals(result.length, 0, "empty input should return empty list");
}

function testSingleIconSingleSize(): void {
  const result = IconProcesser.getInstance().buildIconList([
    makeIcon(["128x128"]),
  ]);
  assertEquals(result.length, 1, "should return one entry");
  assertEquals(result[0].size, 128, "size should be 128");
}

function testSingleIconMultipleSizes(): void {
  const result = IconProcesser.getInstance().buildIconList([
    makeIcon(["128x128", "32x32", "64x64"]),
  ]);
  assertEquals(result.length, 3, "should return three entries");
  assertEquals(result[0].size, 32, "first should be 32");
  assertEquals(result[1].size, 64, "second should be 64");
  assertEquals(result[2].size, 128, "third should be 128");
}

function testAnySizeSortedLast(): void {
  const result = IconProcesser.getInstance().buildIconList([
    makeIcon(["any", "64x64"]),
  ]);
  assertEquals(result.length, 2, "should return two entries");
  assertEquals(result[0].size, 64, "numeric should come first");
  assertEquals(
    result[1].size,
    Number.MAX_SAFE_INTEGER,
    '"any" should map to MAX_SAFE_INTEGER',
  );
}

function testMultipleIconsSorted(): void {
  const iconA = makeIcon(["256x256"]);
  const iconB = makeIcon(["16x16"]);
  const iconC = makeIcon(["48x48"]);
  const result = IconProcesser.getInstance().buildIconList([
    iconA,
    iconB,
    iconC,
  ]);
  assertEquals(result.length, 3, "should return three entries");
  assertEquals(result[0].size, 16, "first should be 16");
  assertEquals(result[1].size, 48, "second should be 48");
  assertEquals(result[2].size, 256, "third should be 256");
}

function testMultipleIconsMultipleSizes(): void {
  const iconA = makeIcon(["128x128", "64x64"]);
  const iconB = makeIcon(["32x32", "256x256"]);
  const result = IconProcesser.getInstance().buildIconList([iconA, iconB]);
  assertEquals(result.length, 4, "should return four entries");
  assertEquals(result[0].size, 32, "first should be 32");
  assertEquals(result[1].size, 64, "second should be 64");
  assertEquals(result[2].size, 128, "third should be 128");
  assertEquals(result[3].size, 256, "fourth should be 256");
}

function testIconReferencePreserved(): void {
  const icon = makeIcon(["48x48"]);
  const result = IconProcesser.getInstance().buildIconList([icon]);
  assert(result[0].icon === icon, "icon reference should be preserved");
}

function testStableSortSameSize(): void {
  const iconA = makeIcon(["64x64"]);
  iconA.src = "a.png";
  const iconB = makeIcon(["64x64"]);
  iconB.src = "b.png";
  const result = IconProcesser.getInstance().buildIconList([iconA, iconB]);
  assertEquals(result.length, 2, "should return two entries");
  // Both have same size — order depends on sort stability (V8/SpiderMonkey are stable)
  assertEquals(result[0].icon.src, "a.png", "first icon should be a.png");
  assertEquals(result[1].icon.src, "b.png", "second icon should be b.png");
}

function testMultipleAnysPreserveOrder(): void {
  const iconA = makeIcon(["any"]);
  iconA.src = "a.png";
  const iconB = makeIcon(["any"]);
  iconB.src = "b.png";
  const result = IconProcesser.getInstance().buildIconList([iconA, iconB]);
  assertEquals(result.length, 2, "should return two entries");
  assertEquals(result[0].icon.src, "a.png", "first 'any' should be a.png");
  assertEquals(result[1].icon.src, "b.png", "second 'any' should be b.png");
}

function testSizeOverflowHandling(): void {
  const result = IconProcesser.getInstance().buildIconList([
    makeIcon(["999999999999999999x999999999999999999"]),
  ]);
  assertEquals(result.length, 1, "should handle overflow gracefully");
  // The parsed size may be NaN or Infinity due to overflow
  // The sort should handle this without crashing
  assertEquals(typeof result[0].size, "number", "size should be number");
}

function testMixedValidAndInvalidSizes(): void {
  const result = IconProcesser.getInstance().buildIconList([
    makeIcon(["64x64"]),
    makeIcon(["invalid"]),
    makeIcon(["128x128"]),
  ]);
  assertEquals(result.length, 3, "should return three entries");
  assertEquals(result[0].size, 64, "first valid should be 64");
  assertEquals(result[2].size, 128, "second valid should be 128");
  // Invalid size will be NaN, which should sort to the end
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "empty array", fn: testEmptyArray },
    { name: "single icon single size", fn: testSingleIconSingleSize },
    { name: "single icon multiple sizes", fn: testSingleIconMultipleSizes },
    { name: '"any" size sorted last', fn: testAnySizeSortedLast },
    { name: "multiple icons sorted", fn: testMultipleIconsSorted },
    {
      name: "multiple icons multiple sizes",
      fn: testMultipleIconsMultipleSizes,
    },
    { name: "icon reference preserved", fn: testIconReferencePreserved },
    { name: "stable sort same size", fn: testStableSortSameSize },
    { name: "multiple 'any's preserve order", fn: testMultipleAnysPreserveOrder },
    { name: "size overflow handling", fn: testSizeOverflowHandling },
    { name: "mixed valid and invalid sizes", fn: testMixedValidAndInvalidSizes },
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
      `iconProcesserBuildIconList.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
