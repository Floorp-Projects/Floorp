// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { HintGenerator } from "../HintGenerator.ts";
import type { ClickableElementInfo } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

/** Create a fake ClickableElementInfo with a given position index */
function makeElement(index: number): ClickableElementInfo {
  return {
    rect: new DOMRect(index, index, 10, 10),
    href: null,
    text: null,
    tagName: "div",
    possibleFalsePositive: false,
  };
}

// ---------------------------------------------------------------------------
// Test: generates correct number of labels
// ---------------------------------------------------------------------------
function testGeneratesCorrectCount(): void {
  const gen = new HintGenerator();
  const elements: ClickableElementInfo[] = Array.from({ length: 5 }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  assertEquals(result.length, 5, "should return 5 hint descriptors for 5 elements");
}

function testGeneratesLargeCount(): void {
  const gen = new HintGenerator();
  const elements: ClickableElementInfo[] = Array.from({ length: 200 }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  assertEquals(result.length, 200, "should return 200 hint descriptors for 200 elements");
}

// ---------------------------------------------------------------------------
// Test: generates single-character labels for small counts (1-11)
// ---------------------------------------------------------------------------
function testSingleCharLabelsForSmallCounts(): void {
  const gen = new HintGenerator();
  for (const count of [1, 5, 10, 11]) {
    const elements: ClickableElementInfo[] = Array.from({ length: count }, (_, i) => makeElement(i));
    const result = gen.generate(elements);
    const allSingle = result.every((desc) => desc.label.length === 1);
    assert(allSingle, `all labels should be single-char for count=${count}`);
  }
}

// ---------------------------------------------------------------------------
// Test: generates multi-character labels for larger counts (12+)
// ---------------------------------------------------------------------------
function testMultiCharLabelsForLargerCounts(): void {
  const gen = new HintGenerator();
  // 12 > 11 (number of chars in "ASDFJKLQWER"), so some must be 2 chars
  const elements: ClickableElementInfo[] = Array.from({ length: 12 }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  const hasMulti = result.some((desc) => desc.label.length >= 2);
  assert(hasMulti, "for 12+ elements, some labels should be 2+ chars");
}

function testVeryLargeCountHasLongLabels(): void {
  const gen = new HintGenerator();
  // 11*11+1 = 122 elements would require 3-char labels
  const count = 122;
  const elements: ClickableElementInfo[] = Array.from({ length: count }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  const hasTriple = result.some((desc) => desc.label.length >= 3);
  assert(hasTriple, `for ${count} elements, some labels should be 3+ chars`);
}

// ---------------------------------------------------------------------------
// Test: all labels are unique
// ---------------------------------------------------------------------------
function testAllLabelsUnique(): void {
  const gen = new HintGenerator();
  for (const count of [1, 11, 50, 200]) {
    const elements: ClickableElementInfo[] = Array.from({ length: count }, (_, i) => makeElement(i));
    const result = gen.generate(elements);
    const labels = result.map((d) => d.label);
    const unique = new Set(labels);
    assertEquals(
      unique.size,
      labels.length,
      `all labels should be unique for count=${count}`,
    );
  }
}

// ---------------------------------------------------------------------------
// Test: labels are from valid character set
// ---------------------------------------------------------------------------
function testLabelsFromValidCharSet(): void {
  const validChars = new Set("ASDFJKLQWER".split(""));
  const gen = new HintGenerator();
  const elements: ClickableElementInfo[] = Array.from({ length: 50 }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  for (const desc of result) {
    for (const ch of desc.label) {
      assert(
        validChars.has(ch),
        `label '${desc.label}' contains invalid char '${ch}'`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Test: returns empty array for zero elements
// ---------------------------------------------------------------------------
function testZeroElements(): void {
  const gen = new HintGenerator();
  const result = gen.generate([]);
  assertEquals(result.length, 0, "should return empty array for empty input");
}

// ---------------------------------------------------------------------------
// Test: first elements get shortest labels
// ---------------------------------------------------------------------------
function testFirstElementsGetShortestLabels(): void {
  const gen = new HintGenerator();
  const count = 20;
  const elements: ClickableElementInfo[] = Array.from({ length: count }, (_, i) => makeElement(i));
  const result = gen.generate(elements);

  // The 11 single-char labels should be among the first 11 positions
  // Since the algorithm sorts alphabetically and reverses, single-char labels
  // will appear first (sorted) then reversed. Let's verify non-decreasing label lengths.
  let prevLength = 0;
  let monotonic = true;
  for (const desc of result) {
    if (desc.label.length < prevLength) {
      monotonic = false;
      break;
    }
    prevLength = desc.label.length;
  }
  assert(monotonic, "label lengths should be non-decreasing (shortest labels first)");
}

// ---------------------------------------------------------------------------
// Test: HintDescriptor properties are correct
// ---------------------------------------------------------------------------
function testDescriptorProperties(): void {
  const gen = new HintGenerator();
  const elements: ClickableElementInfo[] = [
    { rect: new DOMRect(10, 20, 100, 50), href: "http://example.com", text: "Link", tagName: "a", possibleFalsePositive: false },
  ];
  const result = gen.generate(elements);
  assertEquals(result.length, 1, "should return exactly one descriptor");
  assertEquals(result[0].elementIndex, 0, "elementIndex should be 0");
  assertEquals(
    (result[0].rect as DOMRect).x,
    10,
    "rect.x should match element rect",
  );
  assertEquals(
    (result[0].rect as DOMRect).y,
    20,
    "rect.y should match element rect",
  );
}

// ---------------------------------------------------------------------------
// Test: custom character set
// ---------------------------------------------------------------------------
function testCustomCharSet(): void {
  const gen = new HintGenerator("ABC");
  const elements: ClickableElementInfo[] = Array.from({ length: 9 }, (_, i) => makeElement(i));
  const result = gen.generate(elements);
  assertEquals(result.length, 9, "should return 9 descriptors with custom char set");
  const validChars = new Set("ABC".split(""));
  for (const desc of result) {
    for (const ch of desc.label) {
      assert(
        validChars.has(ch),
        `label '${desc.label}' should only contain custom chars`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Test: empty character set returns empty
// ---------------------------------------------------------------------------
function testEmptyCharSet(): void {
  const gen = new HintGenerator("");
  const elements: ClickableElementInfo[] = [makeElement(0)];
  const result = gen.generate(elements);
  assertEquals(result.length, 0, "should return empty array with empty char set");
}

// ---------------------------------------------------------------------------
// Test: labels are unique across batches
// ---------------------------------------------------------------------------
function testUniquenessWithSingleChar(): void {
  const gen = new HintGenerator();
  // Generate labels for 1..11 elements separately; all should be single-char and unique
  for (let count = 1; count <= 11; count++) {
    const elements: ClickableElementInfo[] = Array.from({ length: count }, (_, i) => makeElement(i));
    const result = gen.generate(elements);
    const labels = result.map((d) => d.label);
    const seen = new Set<string>();
    for (const label of labels) {
      assert(!seen.has(label), `label '${label}' duplicated for count=${count}`);
      seen.add(label);
    }
  }
}

// ---------------------------------------------------------------------------
// Test: alphabetical sort-then-reverse produces deterministic labels
// ---------------------------------------------------------------------------
function testDeterministicOutput(): void {
  const gen1 = new HintGenerator();
  const gen2 = new HintGenerator();
  const elements: ClickableElementInfo[] = Array.from({ length: 100 }, (_, i) => makeElement(i));
  const result1 = gen1.generate(elements);
  const result2 = gen2.generate(elements);
  assertEquals(result1.length, result2.length, "both should have same count");
  for (let i = 0; i < result1.length; i++) {
    assertEquals(
      result1[i].label,
      result2[i].label,
      `label at index ${i} should be deterministic`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "generates correct number of labels (5 elements)", fn: testGeneratesCorrectCount },
    { name: "generates correct number of labels (200 elements)", fn: testGeneratesLargeCount },
    { name: "generates single-character labels for small counts (1-11)", fn: testSingleCharLabelsForSmallCounts },
    { name: "generates multi-character labels for larger counts (12+)", fn: testMultiCharLabelsForLargerCounts },
    { name: "generates 3+ char labels for very large counts (122+)", fn: testVeryLargeCountHasLongLabels },
    { name: "all labels are unique", fn: testAllLabelsUnique },
    { name: "labels are from valid character set (ASDFJKLQWER)", fn: testLabelsFromValidCharSet },
    { name: "returns empty array for zero elements", fn: testZeroElements },
    { name: "first elements get shortest labels (non-decreasing lengths)", fn: testFirstElementsGetShortestLabels },
    { name: "HintDescriptor properties match input element", fn: testDescriptorProperties },
    { name: "custom character set works", fn: testCustomCharSet },
    { name: "empty character set returns empty", fn: testEmptyCharSet },
    { name: "labels are unique across batch sizes 1-11", fn: testUniquenessWithSingleChar },
    { name: "output is deterministic across generator instances", fn: testDeterministicOutput },
  ];
  await runTests("HintGenerator.test.ts", tests);
}
