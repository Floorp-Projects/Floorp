// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  buildBackupFile,
  buildBackupSummary,
  collectTabsForWorkspacelessRestore,
  filterBackupFiles,
  formatBackupFileName,
  isBackupFileName,
  isUnassignedSnapshot,
  isWorkspaceBackupFile,
  normalizeBackupConfig,
  parseBackupConfig,
  parseBackupFileName,
  selectExcessPreRestoreBackups,
  selectExpiredBackups,
  serializeBackupConfig,
  sortBackupsNewestFirst,
} from "../utils/workspaces-backup-helpers.ts";
import type { WorkspaceBackupSummary } from "../utils/backup-types.ts";
import type { TWorkspaceID, TWorkspaceSnapshot } from "../utils/type.ts";
import {
  isUnassignedWorkspaceID,
  UNASSIGNED_WORKSPACE_ID,
} from "../utils/workspaces-static-names.ts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const DAY = 24 * 60 * 60 * 1000;

const makeSnapshot = (
  workspaceId: string,
  tabCount: number,
): TWorkspaceSnapshot => ({
  capturedAt: 1700000000000,
  workspace: {
    workspaceId: workspaceId as unknown as TWorkspaceID,
    name: workspaceId,
    userContextId: 0,
  },
  tabs: Array.from({ length: tabCount }, (_, index) => ({
    state: null,
    title: `Tab ${index}`,
    url: `https://example.com/${index}`,
    pinned: false,
    isSelected: index === 0,
    userContextId: 0,
    lastShownWorkspaceId: null,
  })),
});

const makeUnassignedSnapshot = (
  tabCount: number,
): TWorkspaceSnapshot => makeSnapshot(UNASSIGNED_WORKSPACE_ID, tabCount);

const makeSummary = (
  fileName: string,
  kind: WorkspaceBackupSummary["kind"],
  capturedAt: number,
): WorkspaceBackupSummary => ({
  id: fileName,
  kind,
  capturedAt,
  workspaceCount: 1,
  tabCount: 1,
  fileName,
  fileSize: 100,
});

function testFormatAndParseFileName(): void {
  const capturedAt = Date.UTC(2026, 10, 16, 12, 34, 56);
  const fileName = formatBackupFileName(capturedAt, "abcdef12");
  assertEquals(
    fileName,
    "backup-20261116-123456-abcdef12.jsonlz4",
    "file name format",
  );
  const parsed = parseBackupFileName(fileName);
  assert(parsed !== null, "file name should parse");
  assertEquals(parsed?.shortId, "abcdef12", "short id");
  assertEquals(parsed?.timestamp, capturedAt, "timestamp round-trips");
}

function testParseInvalidFileName(): void {
  assertEquals(parseBackupFileName("backup.jsonlz4"), null, "missing stamp");
  assertEquals(
    parseBackupFileName("backup-20261116-123456-ABCDEF12.jsonlz4")?.shortId,
    "ABCDEF12",
    "short id is case-insensitive",
  );
  assertEquals(
    parseBackupFileName("backup-20261116-123456-abcdef12.json"),
    null,
    "wrong extension",
  );
}

function testFilterBackupFiles(): void {
  const paths = [
    "/profile/workspaces/backup/backup-20261116-123456-abcdef12.jsonlz4",
    "/profile/workspaces/backup/backup-20261116-123456-abcdef12.jsonlz4.tmp",
    "/profile/workspaces/backup/notes.txt",
  ];
  const filtered = filterBackupFiles(paths);
  assertEquals(filtered.length, 1, "only final jsonlz4 files");
  assert(isBackupFileName("backup-20261116-123456-abcdef12.jsonlz4"), "valid");
  assert(
    !isBackupFileName("../../backup-20261116-123456-abcdef12.jsonlz4"),
    "path traversal is not a file name",
  );
}

function testBuildSummaryCountsTabs(): void {
  const file = buildBackupFile(
    "manual",
    1700000000000,
    ["ws-1" as unknown as TWorkspaceID],
    "ws-1" as unknown as TWorkspaceID,
    "ws-1" as unknown as TWorkspaceID,
    [makeSnapshot("ws-1", 3), makeSnapshot("ws-2", 2)],
  );
  const summary = buildBackupSummary("backup.jsonlz4", file, 512);
  assertEquals(summary.workspaceCount, 2, "workspace count");
  assertEquals(summary.tabCount, 5, "tab count");
  assertEquals(summary.fileSize, 512, "file size");
  assertEquals(summary.kind, "manual", "kind");
}

function testBuildSummaryExcludesVirtualWorkspaceButCountsItsTabs(): void {
  const realId = "00000000-0000-4000-8000-000000000001";
  const file = buildBackupFile(
    "auto",
    1700000000000,
    [],
    null,
    null,
    [makeSnapshot(realId, 2), makeUnassignedSnapshot(3)],
  );
  const summary = buildBackupSummary("backup.jsonlz4", file, 128);

  assertEquals(
    summary.workspaceCount,
    1,
    "the virtual unassigned snapshot is not a real workspace",
  );
  assertEquals(
    summary.tabCount,
    5,
    "unassigned tabs are included in the tab count",
  );
}

function testUnassignedSnapshotDetection(): void {
  assert(isUnassignedSnapshot(makeUnassignedSnapshot(1)), "sentinel matches");
  assert(
    !isUnassignedSnapshot(
      makeSnapshot("00000000-0000-4000-8000-000000000001", 1),
    ),
    "real workspaces do not match",
  );
}

function testWorkspacelessRestorePrefersUnassignedTabs(): void {
  const unassigned = makeUnassignedSnapshot(3);
  const realId = "00000000-0000-4000-8000-000000000001";
  const file = buildBackupFile(
    "manual",
    1700000000000,
    [],
    null,
    null,
    [makeSnapshot(realId, 2), unassigned],
  );
  const tabs = collectTabsForWorkspacelessRestore(file);

  assertEquals(tabs.length, 3, "only unassigned tabs are restored");
  assert(
    tabs[0] === unassigned.tabs[0],
    "restored tabs come from the virtual unassigned snapshot",
  );
}

function testWorkspacelessRestoreFallsBackToAllTabs(): void {
  const realId = "00000000-0000-4000-8000-000000000001";
  const file = buildBackupFile(
    "auto",
    1700000000000,
    [],
    null,
    null,
    [makeSnapshot(realId, 2)],
  );
  assertEquals(
    collectTabsForWorkspacelessRestore(file).length,
    2,
    "without unassigned tabs, every snapshot's tabs are restored",
  );
}

function testUnassignedWorkspaceIDStillGuardedHere(): void {
  assert(
    isUnassignedWorkspaceID(UNASSIGNED_WORKSPACE_ID),
    "helper re-export stays consistent",
  );
}

function testWorkspaceBackupFileValidation(): void {
  const valid = buildBackupFile(
    "auto",
    1,
    [],
    null,
    null,
    [],
  );
  assert(isWorkspaceBackupFile(valid), "valid file");
  assert(!isWorkspaceBackupFile({ ...valid, version: 2 }), "version");
  assert(!isWorkspaceBackupFile({ ...valid, kind: "other" }), "kind");
  assert(
    !isWorkspaceBackupFile({ ...valid, capturedAt: "now" }),
    "capturedAt",
  );
  assert(!isWorkspaceBackupFile(null), "null");
  assert(!isWorkspaceBackupFile([]), "array");
}

function testExpiredRetentionExcludesPreRestore(): void {
  const now = 10 * DAY;
  const summaries = [
    makeSummary("old-auto", "auto", now - 8 * DAY),
    makeSummary("fresh-auto", "auto", now - 2 * DAY),
    makeSummary("old-manual", "manual", now - 10 * DAY),
    makeSummary("old-pre", "pre-restore", now - 30 * DAY),
  ];
  const expired = selectExpiredBackups(summaries, 7, now);
  assertEquals(expired.length, 2, "two age-expired backups");
  assert(expired.some((s) => s.fileName === "old-auto"), "old auto");
  assert(expired.some((s) => s.fileName === "old-manual"), "old manual");
  assert(
    !expired.some((s) => s.fileName === "old-pre"),
    "pre-restore is exempt",
  );
}

function testExpiredRetentionInvalidDays(): void {
  const summaries = [makeSummary("old", "auto", 0)];
  assertEquals(selectExpiredBackups(summaries, -1, DAY).length, 0, "negative");
  assertEquals(
    selectExpiredBackups(summaries, Number.NaN, DAY).length,
    0,
    "NaN",
  );
}

function testPreRestoreCapKeepsNewest(): void {
  const summaries = [
    makeSummary("pre-1", "pre-restore", 1),
    makeSummary("pre-2", "pre-restore", 2),
    makeSummary("pre-3", "pre-restore", 3),
    makeSummary("auto", "auto", 4),
  ];
  const excess = selectExcessPreRestoreBackups(summaries, 2);
  assertEquals(excess.length, 1, "one excess pre-restore");
  assertEquals(excess[0].fileName, "pre-1", "oldest is removed");
}

function testPreRestoreCapZero(): void {
  const summaries = [makeSummary("pre-1", "pre-restore", 1)];
  assertEquals(
    selectExcessPreRestoreBackups(summaries, 0).length,
    1,
    "zero keeps none",
  );
  assertEquals(
    selectExcessPreRestoreBackups(summaries, -1).length,
    0,
    "negative disables cleanup",
  );
}

function testSortNewestFirst(): void {
  const summaries = [
    makeSummary("a", "auto", 1),
    makeSummary("b", "auto", 3),
    makeSummary("c", "auto", 2),
  ];
  const sorted = sortBackupsNewestFirst(summaries);
  assertEquals(sorted.map((s) => s.fileName).join(","), "b,c,a", "sorted");
  assertEquals(summaries[0].fileName, "a", "input is not mutated");
}

function testNormalizeConfigDefaultsAndOverrides(): void {
  const defaults = normalizeBackupConfig(null);
  assertEquals(defaults.enabled, true, "enabled default");
  assertEquals(defaults.intervalMinutes, 15, "interval default");
  assertEquals(defaults.retentionDays, 7, "retention default");
  assertEquals(defaults.maxPreRestoreBackups, 10, "pre-restore default");

  const custom = normalizeBackupConfig({
    enabled: false,
    intervalMinutes: 30,
    retentionDays: 0,
    maxPreRestoreBackups: 3,
  });
  assertEquals(custom.enabled, false, "enabled override");
  assertEquals(custom.intervalMinutes, 30, "interval override");
  assertEquals(custom.retentionDays, 0, "retention allows zero");
  assertEquals(custom.maxPreRestoreBackups, 3, "cap override");

  const invalid = normalizeBackupConfig({
    enabled: "yes",
    intervalMinutes: -5,
    retentionDays: Number.NaN,
    maxPreRestoreBackups: -1,
  });
  assertEquals(invalid.enabled, true, "invalid enabled falls back");
  assertEquals(invalid.intervalMinutes, 15, "invalid interval falls back");
  assertEquals(invalid.retentionDays, 7, "invalid retention falls back");
  assertEquals(
    invalid.maxPreRestoreBackups,
    10,
    "invalid cap falls back",
  );
}

function testConfigRoundTrip(): void {
  const config = normalizeBackupConfig({
    enabled: false,
    intervalMinutes: 60,
    retentionDays: 14,
    maxPreRestoreBackups: 5,
  });
  const parsed = parseBackupConfig(serializeBackupConfig(config));
  assertEquals(parsed.enabled, false, "enabled round-trip");
  assertEquals(parsed.intervalMinutes, 60, "interval round-trip");
  assertEquals(parsed.retentionDays, 14, "retention round-trip");
  assertEquals(parsed.maxPreRestoreBackups, 5, "cap round-trip");
}

function testParseConfigInvalidJson(): void {
  const parsed = parseBackupConfig("{not json");
  assertEquals(parsed.enabled, true, "falls back to defaults");
  assertEquals(parsed.intervalMinutes, 15, "default interval");
}

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "file name format and parse", fn: testFormatAndParseFileName },
    { name: "invalid file names", fn: testParseInvalidFileName },
    { name: "filter backup files", fn: testFilterBackupFiles },
    { name: "summary counts tabs", fn: testBuildSummaryCountsTabs },
    {
      name: "summary excludes virtual workspace but counts its tabs",
      fn: testBuildSummaryExcludesVirtualWorkspaceButCountsItsTabs,
    },
    {
      name: "unassigned snapshot detection",
      fn: testUnassignedSnapshotDetection,
    },
    {
      name: "workspace-less restore prefers unassigned tabs",
      fn: testWorkspacelessRestorePrefersUnassignedTabs,
    },
    {
      name: "workspace-less restore falls back to all tabs",
      fn: testWorkspacelessRestoreFallsBackToAllTabs,
    },
    {
      name: "unassigned workspace ID guard (backup helpers)",
      fn: testUnassignedWorkspaceIDStillGuardedHere,
    },
    { name: "backup file validation", fn: testWorkspaceBackupFileValidation },
    {
      name: "expired retention excludes pre-restore",
      fn: testExpiredRetentionExcludesPreRestore,
    },
    {
      name: "expired retention invalid days",
      fn: testExpiredRetentionInvalidDays,
    },
    { name: "pre-restore cap keeps newest", fn: testPreRestoreCapKeepsNewest },
    { name: "pre-restore cap zero", fn: testPreRestoreCapZero },
    { name: "sort newest first", fn: testSortNewestFirst },
    {
      name: "normalize config defaults and overrides",
      fn: testNormalizeConfigDefaultsAndOverrides,
    },
    { name: "config round trip", fn: testConfigRoundTrip },
    { name: "parse config invalid json", fn: testParseConfigInvalidJson },
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
      `workspacesBackupHelpers.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
