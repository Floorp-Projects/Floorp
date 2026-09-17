// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  createWorkspaceSnapshotMetadata,
  extractUrlFromState,
  toNumber,
} from "../utils/workspace-snapshot.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

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
  assertEquals(toNumber(""), 0, "empty string should return fallback 0");
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
    entries: [{ url: "https://first.com" }, { url: "https://second.com" }],
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
    entries: [{ url: "https://first.com" }, { url: "https://last.com" }],
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

function testExtractUrlEntryWithoutUrl(): void {
  const state = {
    entries: [{ title: "No URL" }, { url: "https://second.com" }],
    index: 2,
  };
  assertEquals(
    extractUrlFromState(state),
    "https://second.com",
    "should skip entries without URL",
  );
}

function testExtractUrlZeroIndex(): void {
  const state = {
    entries: [{ url: "https://first.com" }, { url: "https://second.com" }],
    index: 0,
  };
  // Falls back to last entry when index is 0
  assertEquals(
    extractUrlFromState(state),
    "https://second.com",
    "index=0 should fall back to last entry",
  );
}

function testExtractUrlNullEntries(): void {
  const state = {
    entries: null,
  };
  assertEquals(
    extractUrlFromState(state),
    null,
    "null entries should return null",
  );
}

function testExtractUrlEntryWithNullUrl(): void {
  const state = {
    entries: [{ url: null }, { url: "https://valid.com" }],
  };
  assertEquals(
    extractUrlFromState(state),
    "https://valid.com",
    "should skip entries with null URL",
  );
}

function testExtractUrlComplexEntries(): void {
  const state = {
    entries: [
      { url: "https://first.com", title: "First" },
      { url: "https://second.com", title: "Second" },
      { url: "https://third.com", title: "Third" },
    ],
    index: 2,
  };
  assertEquals(
    extractUrlFromState(state),
    "https://second.com",
    "should handle complex entry objects",
  );
}

function testSnapshotMetadataPreservesRawIconCategories(): void {
  const workspaceId = "00000000-0000-4000-8000-000000000001" as TWorkspaceID;
  const base = { name: "Snapshot", userContextId: 7 };
  const cases: Array<
    [string, { name: string; userContextId: number; icon?: string | null }]
  > = [
    ["absent", base],
    ["own undefined", { ...base, icon: undefined }],
    ["null", { ...base, icon: null }],
    ["alias", { ...base, icon: "article" }],
    ["canonical", { ...base, icon: "floorp-icon:v1:article" }],
    ["opaque", { ...base, icon: "future:value" }],
    ["URI", { ...base, icon: "https://example.invalid/icon.svg" }],
  ];
  for (const [label, workspace] of cases) {
    const metadata = createWorkspaceSnapshotMetadata(workspaceId, workspace);
    assertEquals(
      Object.hasOwn(metadata, "icon"),
      Object.hasOwn(workspace, "icon"),
      `${label} presence`,
    );
    if (Object.hasOwn(workspace, "icon")) {
      assertEquals(metadata.icon, workspace.icon, `${label} value`);
    }
    assertEquals(metadata.userContextId, 7, `${label} user context`);
  }

  const ownUndefined = createWorkspaceSnapshotMetadata(workspaceId, {
    ...base,
    icon: undefined,
  });
  const json = JSON.parse(JSON.stringify(ownUndefined)) as Record<string, unknown>;
  assert(
    !Object.hasOwn(json, "icon"),
    "snapshot JSON omits own undefined instead of producing null",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export function runAllTests(): void {
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
    { name: "extractUrl entry without URL", fn: testExtractUrlEntryWithoutUrl },
    { name: "extractUrl zero index", fn: testExtractUrlZeroIndex },
    { name: "extractUrl null entries", fn: testExtractUrlNullEntries },
    { name: "extractUrl entry with null URL", fn: testExtractUrlEntryWithNullUrl },
    { name: "extractUrl complex entries", fn: testExtractUrlComplexEntries },
    {
      name: "snapshot metadata preserves raw icon categories",
      fn: testSnapshotMetadataPreservesRawIconCategories,
    },
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
