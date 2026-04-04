// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  toNumber,
  extractUrlFromState,
} from "../../common/workspaces/utils/workspace-snapshot.ts";

type TestCase = {
  name: string;
  fn: () => void;
};

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — toNumber
// ---------------------------------------------------------------------------

function testToNumberValidString(): void {
  assertEquals(toNumber("42"), 42, '"42" should return 42');
}

function testToNumberNull(): void {
  assertEquals(toNumber(null), 0, "null should return default 0");
}

function testToNumberNaN(): void {
  assertEquals(toNumber("abc"), 0, '"abc" should return fallback 0');
}

function testToNumberCustomFallback(): void {
  assertEquals(toNumber(null, 5), 5, "null with fallback=5 should return 5");
}

function testToNumberEmptyString(): void {
  assertEquals(toNumber(""), 0, 'empty string should return fallback 0');
}

function testToNumberZero(): void {
  assertEquals(toNumber("0"), 0, '"0" should return 0');
}

function testToNumberNegative(): void {
  assertEquals(toNumber("-3"), -3, '"-3" should return -3');
}

// ---------------------------------------------------------------------------
// Tests — extractUrlFromState
// ---------------------------------------------------------------------------

function testExtractUrlWithIndex(): void {
  const state = {
    entries: [
      { url: "https://first.com" },
      { url: "https://second.com" },
    ],
    index: 2,
  };
  assertEquals(
    extractUrlFromState(state),
    "https://second.com",
    "should return URL at (index-1)",
  );
}

function testExtractUrlNoIndex(): void {
  const state = {
    entries: [
      { url: "https://first.com" },
      { url: "https://last.com" },
    ],
  };
  assertEquals(
    extractUrlFromState(state),
    "https://last.com",
    "missing index should fall back to last entry",
  );
}

function testExtractUrlNoEntries(): void {
  const state = {};
  assertEquals(
    extractUrlFromState(state),
    null,
    "missing entries should return null",
  );
}

function testExtractUrlEmptyEntries(): void {
  const state = { entries: [] };
  assertEquals(
    extractUrlFromState(state),
    null,
    "empty entries should return null",
  );
}

function testExtractUrlIndexOutOfBounds(): void {
  const state = {
    entries: [{ url: "https://only.com" }],
    index: 5, // out of bounds
  };
  // Falls back to iterating from end
  assertEquals(
    extractUrlFromState(state),
    "https://only.com",
    "out-of-bounds index should fall back to last entry",
  );
}

function testExtractUrlIndexOne(): void {
  const state = {
    entries: [{ url: "https://first.com" }],
    index: 1,
  };
  assertEquals(
    extractUrlFromState(state),
    "https://first.com",
    "index=1 should return first entry",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "toNumber valid string", fn: testToNumberValidString },
    { name: "toNumber null", fn: testToNumberNull },
    { name: "toNumber NaN", fn: testToNumberNaN },
    { name: "toNumber custom fallback", fn: testToNumberCustomFallback },
    { name: "toNumber empty string", fn: testToNumberEmptyString },
    { name: "toNumber zero", fn: testToNumberZero },
    { name: "toNumber negative", fn: testToNumberNegative },
    { name: "extractUrl with index", fn: testExtractUrlWithIndex },
    { name: "extractUrl no index", fn: testExtractUrlNoIndex },
    { name: "extractUrl no entries", fn: testExtractUrlNoEntries },
    { name: "extractUrl empty entries", fn: testExtractUrlEmptyEntries },
    {
      name: "extractUrl index out of bounds",
      fn: testExtractUrlIndexOutOfBounds,
    },
    { name: "extractUrl index=1", fn: testExtractUrlIndexOne },
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
      `workspaceSnapshotUtils.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
