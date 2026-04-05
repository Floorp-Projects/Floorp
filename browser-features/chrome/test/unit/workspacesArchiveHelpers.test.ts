// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  buildSummary,
  filterJsonFiles,
  isRecord,
} from "../../common/workspaces/utils/workspaces-archive-service.ts";
import type { TWorkspaceSnapshot } from "../../common/workspaces/utils/type.ts";
import { type TestCase, assert, assertEquals } from "../utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Tests — isRecord
// ---------------------------------------------------------------------------

function testIsRecordObject(): void {
  assert(isRecord({ a: 1 }), "plain object should be a record");
}

function testIsRecordEmpty(): void {
  assert(isRecord({}), "empty object should be a record");
}

function testIsRecordNull(): void {
  assertEquals(isRecord(null), false, "null should not be a record");
}

function testIsRecordUndefined(): void {
  assertEquals(isRecord(undefined), false, "undefined should not be a record");
}

function testIsRecordArray(): void {
  // Note: isRecord uses `typeof value === "object" && value !== null`
  // which returns true for arrays — this is by design in the implementation
  assertEquals(isRecord([1, 2]), true, "array is technically an object record");
}

function testIsRecordString(): void {
  assertEquals(isRecord("string"), false, "string should not be a record");
}

function testIsRecordNumber(): void {
  assertEquals(isRecord(42), false, "number should not be a record");
}

// ---------------------------------------------------------------------------
// Tests — filterJsonFiles
// ---------------------------------------------------------------------------

function testFilterJsonFilesBasic(): void {
  const input = ["a.json", "b.txt", "c.json", "d.png"];
  const result = filterJsonFiles(input);
  assertEquals(result.length, 2, "should filter to 2 JSON files");
  assert(result.includes("a.json"), "should include a.json");
  assert(result.includes("c.json"), "should include c.json");
}

function testFilterJsonFilesEmpty(): void {
  assertEquals(filterJsonFiles([]).length, 0, "empty input → empty output");
}

function testFilterJsonFilesNone(): void {
  assertEquals(
    filterJsonFiles(["a.txt", "b.png"]).length,
    0,
    "no JSON files → empty",
  );
}

function testFilterJsonFilesCaseInsensitive(): void {
  const result = filterJsonFiles(["A.JSON", "b.Json"]);
  assertEquals(result.length, 2, "should match case-insensitively");
}

// ---------------------------------------------------------------------------
// Tests — buildSummary
// ---------------------------------------------------------------------------

function testBuildSummaryBasic(): void {
  const snapshot: TWorkspaceSnapshot = {
    capturedAt: 1700000000000,
    workspace: {
      workspaceId: "ws-1",
      name: "Work",
      icon: "briefcase",
      userContextId: 0,
    },
    tabs: [
      {
        state: null,
        title: "Tab 1",
        url: "https://example.com",
        pinned: false,
        isSelected: false,
        userContextId: 0,
        lastShownWorkspaceId: null,
      },
      {
        state: null,
        title: "Tab 2",
        url: "https://example.org",
        pinned: true,
        isSelected: true,
        userContextId: 0,
        lastShownWorkspaceId: null,
      },
    ],
  };

  const summary = buildSummary("archive-1", snapshot, "/path/to/file.json");
  assertEquals(summary.archiveId, "archive-1", "archiveId");
  assertEquals(summary.workspaceId, "ws-1", "workspaceId");
  assertEquals(summary.name, "Work", "name");
  assertEquals(summary.icon, "briefcase", "icon");
  assertEquals(summary.userContextId, 0, "userContextId");
  assertEquals(summary.capturedAt, 1700000000000, "capturedAt");
  assertEquals(summary.filePath, "/path/to/file.json", "filePath");
  assertEquals(summary.tabCount, 2, "tabCount");
}

function testBuildSummaryNullIcon(): void {
  const snapshot: TWorkspaceSnapshot = {
    capturedAt: 0,
    workspace: {
      workspaceId: "ws-2",
      name: "Empty",
      icon: null,
      userContextId: 1,
    },
    tabs: [],
  };

  const summary = buildSummary("id", snapshot, "/p");
  assertEquals(summary.icon, null, "null icon should pass through");
  assertEquals(summary.tabCount, 0, "zero tabs");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "isRecord object", fn: testIsRecordObject },
    { name: "isRecord empty", fn: testIsRecordEmpty },
    { name: "isRecord null", fn: testIsRecordNull },
    { name: "isRecord undefined", fn: testIsRecordUndefined },
    { name: "isRecord array", fn: testIsRecordArray },
    { name: "isRecord string", fn: testIsRecordString },
    { name: "isRecord number", fn: testIsRecordNumber },
    { name: "filterJsonFiles basic", fn: testFilterJsonFilesBasic },
    { name: "filterJsonFiles empty", fn: testFilterJsonFilesEmpty },
    { name: "filterJsonFiles none", fn: testFilterJsonFilesNone },
    { name: "filterJsonFiles case", fn: testFilterJsonFilesCaseInsensitive },
    { name: "buildSummary basic", fn: testBuildSummaryBasic },
    { name: "buildSummary null icon", fn: testBuildSummaryNullIcon },
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
      `workspacesArchiveHelpers.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
