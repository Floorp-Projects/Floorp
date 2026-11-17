// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  buildBackupFile,
  buildBackupSummary,
  filterBackupFiles,
  findDanglingWorkspaceReferences,
  formatBackupFileName,
  isBackupFileName,
  isWorkspaceBackupFile,
  normalizeBackupConfig,
  parseBackupConfig,
  parseBackupFileName,
  sanitizeTabState,
  selectExcessPreRestoreBackups,
  selectExpiredBackups,
  serializeBackupConfig,
  sortBackupsNewestFirst,
} from "../utils/workspaces-backup-helpers.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupState,
  WorkspaceBackupSummary,
  WorkspaceBackupTab,
  WorkspaceBackupWindow,
} from "../utils/backup-types.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const DAY = 24 * 60 * 60 * 1000;
const WS_A = "00000000-0000-4000-8000-000000000001" as TWorkspaceID;
const WS_B = "00000000-0000-4000-8000-000000000002" as TWorkspaceID;

const makeTab = (
  index: number,
  overrides: Partial<WorkspaceBackupTab> = {},
): WorkspaceBackupTab => ({
  state: null,
  title: `Tab ${index}`,
  url: `https://example.com/${index}`,
  pinned: false,
  isSelected: index === 0,
  userContextId: 0,
  lastShownWorkspaceId: null,
  ...overrides,
});

const makeWindow = (
  tabCount: number,
  isFocused = false,
): WorkspaceBackupWindow => ({
  isFocused,
  tabs: Array.from({ length: tabCount }, (_, index) => makeTab(index)),
});

const makeWorkspaceState = (): WorkspaceBackupState => ({
  workspaces: [
    { workspaceId: WS_A, name: "A", userContextId: 0, icon: null },
    { workspaceId: WS_B, name: "B", userContextId: 0, icon: null },
  ],
  order: [WS_A, WS_B],
  defaultID: WS_A,
  currentWorkspaceId: WS_A,
});

const makeFile = (
  overrides: Partial<WorkspaceBackupFile> = {},
): WorkspaceBackupFile => ({
  version: 2,
  kind: "manual",
  mode: "workspaces",
  capturedAt: 1700000000000,
  windows: [makeWindow(2, true)],
  workspaceState: makeWorkspaceState(),
  ...overrides,
});

const makeSummary = (
  fileName: string,
  kind: WorkspaceBackupSummary["kind"],
  capturedAt: number,
): WorkspaceBackupSummary => ({
  id: fileName,
  kind,
  mode: "tabs",
  capturedAt,
  windowCount: 1,
  workspaceCount: 0,
  tabCount: 1,
  fileName,
  fileSize: 100,
});

/* ------------------------------------------------------------------ */

function testFormatAndParseFileName(): void {
  const capturedAt = Date.UTC(2026, 10, 17, 8, 30, 15);
  const fileName = formatBackupFileName(capturedAt, "abcdef01");
  assertEquals(fileName, "backup-20261117-083015-abcdef01.jsonlz4", "name");
  assert(isBackupFileName(fileName), "name is a backup name");

  const parsed = parseBackupFileName(fileName);
  assert(parsed !== null, "parsed");
  assertEquals(parsed!.shortId, "abcdef01", "short id");
  assertEquals(parsed!.timestamp, capturedAt, "timestamp round trip");
}

function testParseInvalidFileName(): void {
  for (
    const name of [
      "backup-20261117-083015-abcdef01.json",
      "backup-2026111-083015-abcdef01.jsonlz4",
      "other-20261117-083015-abcdef01.jsonlz4",
      "backup-20261117-083015-abcdef01.jsonlz4\n",
      "../backup-20261117-083015-abcdef01.jsonlz4",
    ]
  ) {
    assert(!isBackupFileName(name), `rejects ${JSON.stringify(name)}`);
  }
}

function testFilterBackupFiles(): void {
  const filtered = filterBackupFiles([
    "/profile/workspaces/backup/backup-20261117-083015-abcdef01.jsonlz4",
    "/profile/workspaces/backup/notes.txt",
    "/profile/workspaces/backup/backup-20261117-083015-abcdef01.jsonlz4.tmp",
  ]);
  assertEquals(filtered.length, 1, "only the real backup is kept");
}

function testSummaryCountsWindowsAndTabs(): void {
  const file = makeFile({
    windows: [makeWindow(2, true), makeWindow(3)],
  });
  const summary = buildBackupSummary("backup-x.jsonlz4", file, 4096);
  assertEquals(summary.windowCount, 2, "window count");
  assertEquals(summary.tabCount, 5, "tab count");
  assertEquals(summary.workspaceCount, 2, "workspace count");
  assertEquals(summary.mode, "workspaces", "mode");
  assertEquals(summary.fileSize, 4096, "file size");
}

function testSummaryTabsModeHasNoWorkspaces(): void {
  const file = makeFile({
    mode: "tabs",
    workspaceState: undefined,
    windows: [makeWindow(4, true)],
  });
  const summary = buildBackupSummary("backup-y.jsonlz4", file, 10);
  assertEquals(summary.workspaceCount, 0, "no workspaces in tabs mode");
  assertEquals(summary.windowCount, 1, "window count");
  assertEquals(summary.tabCount, 4, "tab count");
}

function testValidatesWorkspacesMode(): void {
  assert(isWorkspaceBackupFile(makeFile()), "valid workspaces file");
}

function testValidatesTabsMode(): void {
  const file = makeFile({ mode: "tabs", workspaceState: undefined });
  assert(isWorkspaceBackupFile(file), "valid tabs file");
}

function testRejectsInvalidFiles(): void {
  const cases: Array<[string, unknown]> = [
    ["v1", { ...makeFile(), version: 1 }],
    ["unknown mode", { ...makeFile(), mode: "everything" }],
    ["tabs with workspaceState", makeFile({ mode: "tabs" })],
    [
      "workspaces without workspaceState",
      { ...makeFile(), workspaceState: undefined },
    ],
    ["empty windows", makeFile({ windows: [] })],
    [
      "focused flag missing",
      makeFile({
        windows: [{ tabs: [makeTab(0)] } as unknown as WorkspaceBackupWindow],
      }),
    ],
    [
      "tab with bad userContextId",
      makeFile({
        windows: [{
          isFocused: true,
          tabs: [makeTab(0, { userContextId: -1 })],
        }],
      }),
    ],
    [
      "tab with bad state type",
      makeFile({
        windows: [{
          isFocused: true,
          tabs: [
            { ...makeTab(0), state: "nope" } as unknown as WorkspaceBackupTab,
          ],
        }],
      }),
    ],
    ["null", null],
    ["array", []],
  ];
  for (const [label, value] of cases) {
    assert(!isWorkspaceBackupFile(value), `rejects ${label}`);
  }
}

function testDanglingWorkspaceReferences(): void {
  const file = makeFile({
    windows: [{
      isFocused: true,
      tabs: [
        makeTab(0, { workspaceId: WS_A }),
        makeTab(1, {
          workspaceId: "00000000-0000-4000-8000-00000000dead" as TWorkspaceID,
        }),
      ],
    }],
  });
  const dangling = findDanglingWorkspaceReferences(file);
  assertEquals(dangling.length, 1, "one dangling reference");
  assertEquals(
    dangling[0],
    "00000000-0000-4000-8000-00000000dead",
    "dangling id",
  );
  // The file itself stays valid; the tab is treated as unattributed.
  assert(isWorkspaceBackupFile(file), "file remains valid");
}

function testSanitizeStripsSensitiveState(): void {
  const sanitized = sanitizeTabState({
    entries: [{
      url: "https://example.com",
      // Nested occurrences must be removed too, not just the top-level keys.
      formdata: { field: "secret" },
      scroll: "0,10",
    }],
    index: 1,
    formdata: { field: "secret" },
    storage: { key: "value" },
    scroll: "0,120",
    floorpWorkspaceId: WS_A,
    floorpLastShowWorkspaceId: WS_B,
    floorpDisableHistory: "true",
    attributes: {
      floorpWorkspaceId: WS_A,
      floorpLastShowWorkspaceId: WS_B,
      image: "https://example.com/icon.png",
    },
  });
  assert(sanitized !== null, "sanitized");
  assert(!("formdata" in sanitized!), "formdata removed");
  assert(!("storage" in sanitized!), "storage removed");
  assert(!("scroll" in sanitized!), "scroll removed");
  assert(
    !("floorpWorkspaceId" in sanitized!),
    "top-level workspace id removed",
  );
  assert(
    !("floorpDisableHistory" in sanitized!),
    "disable-history marker removed",
  );
  const attributes = sanitized!.attributes as Record<string, unknown>;
  assert(
    !("floorpWorkspaceId" in attributes),
    "attribute workspace id removed",
  );
  assertEquals(
    attributes.image,
    "https://example.com/icon.png",
    "unrelated attributes kept",
  );
  assertEquals(sanitized!.index, 1, "unrelated state kept");
  const entries = sanitized!.entries as Array<Record<string, unknown>>;
  assert(!("formdata" in entries[0]), "nested formdata removed");
  assert(!("scroll" in entries[0]), "nested scroll removed");
  assertEquals(entries[0].url, "https://example.com", "nested url kept");
}

function testSanitizeKeepsNull(): void {
  assertEquals(sanitizeTabState(null), null, "null stays null");
}

function testBuildBackupFileTabsMode(): void {
  const file = buildBackupFile(
    "auto",
    "tabs",
    1700000000000,
    [makeWindow(1, true)],
  );
  assertEquals(file.version, 2, "version");
  assertEquals(file.mode, "tabs", "mode");
  assert(!("workspaceState" in file), "tabs mode carries no workspace state");
  assert(isWorkspaceBackupFile(file), "valid");
}

function testBuildBackupFileWorkspacesMode(): void {
  const file = buildBackupFile(
    "manual",
    "workspaces",
    1700000000000,
    [makeWindow(1, true)],
    makeWorkspaceState(),
  );
  assert("workspaceState" in file, "workspace state present");
  assert(isWorkspaceBackupFile(file), "valid");
}

function testExpiredRetentionExcludesPreRestore(): void {
  const now = 1700000000000;
  const summaries = [
    makeSummary("old-auto.jsonlz4", "auto", now - 10 * DAY),
    makeSummary("old-manual.jsonlz4", "manual", now - 10 * DAY),
    makeSummary("old-pre.jsonlz4", "pre-restore", now - 10 * DAY),
    makeSummary("new-auto.jsonlz4", "auto", now - 1 * DAY),
  ];
  const expired = selectExpiredBackups(summaries, 7, now);
  assertEquals(expired.length, 2, "two expired");
  assert(
    expired.every((entry) => entry.kind !== "pre-restore"),
    "pre-restore is exempt",
  );
}

function testExpiredRetentionInvalidDays(): void {
  const now = 1700000000000;
  const summaries = [makeSummary("a.jsonlz4", "auto", now - 100 * DAY)];
  assertEquals(selectExpiredBackups(summaries, -1, now).length, 0, "negative");
  assertEquals(
    selectExpiredBackups(summaries, Number.NaN, now).length,
    0,
    "NaN",
  );
}

function testPreRestoreCapKeepsNewest(): void {
  const now = 1700000000000;
  const summaries = [
    makeSummary("p1.jsonlz4", "pre-restore", now - 4 * DAY),
    makeSummary("p2.jsonlz4", "pre-restore", now - 3 * DAY),
    makeSummary("p3.jsonlz4", "pre-restore", now - 2 * DAY),
    makeSummary("p4.jsonlz4", "pre-restore", now - 1 * DAY),
  ];
  const excess = selectExcessPreRestoreBackups(summaries, 2);
  assertEquals(excess.length, 2, "two excess");
  assertEquals(excess[0].fileName, "p1.jsonlz4", "oldest first");
  assertEquals(excess[1].fileName, "p2.jsonlz4", "second oldest");
}

function testPreRestoreCapZero(): void {
  const now = 1700000000000;
  const summaries = [makeSummary("p1.jsonlz4", "pre-restore", now)];
  assertEquals(
    selectExcessPreRestoreBackups(summaries, 0).length,
    1,
    "zero keeps none",
  );
}

function testSortNewestFirst(): void {
  const sorted = sortBackupsNewestFirst([
    makeSummary("a.jsonlz4", "auto", 1),
    makeSummary("b.jsonlz4", "auto", 3),
    makeSummary("c.jsonlz4", "auto", 2),
  ]);
  assertEquals(
    sorted.map((entry) => entry.fileName).join(","),
    "b.jsonlz4,c.jsonlz4,a.jsonlz4",
    "sorted",
  );
}

function testNormalizeConfigDefaultsAndOverrides(): void {
  const defaults = normalizeBackupConfig({});
  assertEquals(defaults.enabled, true, "enabled default");
  assertEquals(defaults.intervalMinutes, 15, "interval default");
  assertEquals(defaults.retentionDays, 7, "retention default");
  assertEquals(defaults.maxPreRestoreBackups, 10, "pre-restore default");

  const overridden = normalizeBackupConfig({
    enabled: false,
    intervalMinutes: 30,
    retentionDays: 0,
    maxPreRestoreBackups: 0,
  });
  assertEquals(overridden.enabled, false, "enabled override");
  assertEquals(overridden.intervalMinutes, 30, "interval override");
  assertEquals(overridden.retentionDays, 0, "retention override");
  assertEquals(overridden.maxPreRestoreBackups, 0, "pre-restore override");

  const invalid = normalizeBackupConfig({
    intervalMinutes: -5,
    retentionDays: Number.NaN,
  });
  assertEquals(invalid.intervalMinutes, 15, "invalid interval falls back");
  assertEquals(invalid.retentionDays, 7, "invalid retention falls back");
}

function testConfigRoundTrip(): void {
  const config = {
    enabled: false,
    intervalMinutes: 60,
    retentionDays: 3,
    maxPreRestoreBackups: 2,
  };
  const parsed = parseBackupConfig(serializeBackupConfig(config));
  assertEquals(parsed.enabled, config.enabled, "enabled");
  assertEquals(parsed.intervalMinutes, config.intervalMinutes, "interval");
  assertEquals(parsed.retentionDays, config.retentionDays, "retention");
  assertEquals(
    parsed.maxPreRestoreBackups,
    config.maxPreRestoreBackups,
    "pre-restore",
  );
}

function testParseConfigInvalidJson(): void {
  const parsed = parseBackupConfig("{not json");
  assertEquals(parsed.enabled, true, "falls back to defaults");
  assertEquals(parsed.intervalMinutes, 15, "interval default");
}

/* ------------------------------------------------------------------ */

export function runAllTests(): void {
  const tests: TestCase[] = [
    { name: "file name format and parse", fn: testFormatAndParseFileName },
    { name: "invalid file names", fn: testParseInvalidFileName },
    { name: "filter backup files", fn: testFilterBackupFiles },
    {
      name: "summary counts windows and tabs",
      fn: testSummaryCountsWindowsAndTabs,
    },
    {
      name: "summary tabs mode has no workspaces",
      fn: testSummaryTabsModeHasNoWorkspaces,
    },
    { name: "validates workspaces mode", fn: testValidatesWorkspacesMode },
    { name: "validates tabs mode", fn: testValidatesTabsMode },
    { name: "rejects invalid files", fn: testRejectsInvalidFiles },
    {
      name: "dangling workspace references",
      fn: testDanglingWorkspaceReferences,
    },
    {
      name: "sanitize strips sensitive state",
      fn: testSanitizeStripsSensitiveState,
    },
    { name: "sanitize keeps null", fn: testSanitizeKeepsNull },
    { name: "build file tabs mode", fn: testBuildBackupFileTabsMode },
    {
      name: "build file workspaces mode",
      fn: testBuildBackupFileWorkspacesMode,
    },
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
