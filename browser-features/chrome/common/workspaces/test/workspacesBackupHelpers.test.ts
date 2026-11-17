// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  buildBackupFile,
  buildBackupSummary,
  filterBackupFiles,
  findDanglingWorkspaceReferences,
  formatBackupFileName,
  isBackupFileName,
  isRestorableTabUrl,
  isWorkspaceBackupFile,
  MAX_CAPTURED_AT,
  MIN_KEEP_BACKUPS,
  normalizeBackupConfig,
  parseBackupConfig,
  parseBackupFileName,
  sanitizeTabState,
  selectExcessPreRestoreBackups,
  selectExpiredBackups,
  serializeBackupConfig,
  sortBackupsNewestFirst,
} from "../utils/workspaces-backup-helpers.ts";
import { WORKSPACE_BACKUP_LIMITS } from "../utils/backup-types.ts";
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
      floorpWorkspaceLastShowId: WS_B,
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
  assert(
    !("floorpWorkspaceLastShowId" in attributes),
    "attribute last-shown workspace id removed",
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
  // minKeep 0 isolates the age rule from the keep-newest rule.
  const expired = selectExpiredBackups(summaries, 7, now, 0);
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
  // The raw selector still honours 0, but a config can no longer produce it.
  assertEquals(
    selectExcessPreRestoreBackups(summaries, 0).length,
    1,
    "selector with zero keeps none",
  );
  assertEquals(
    normalizeBackupConfig({ maxPreRestoreBackups: 0 }).maxPreRestoreBackups,
    1,
    "config clamps 0 to 1",
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
    retentionDays: 2,
    maxPreRestoreBackups: 4,
  });
  assertEquals(overridden.enabled, false, "enabled override");
  assertEquals(overridden.intervalMinutes, 30, "interval override");
  assertEquals(overridden.retentionDays, 2, "retention override");
  assertEquals(overridden.maxPreRestoreBackups, 4, "pre-restore override");

  const invalid = normalizeBackupConfig({
    intervalMinutes: "5",
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

function testFractionalRetentionCapIsFloored(): void {
  const config = normalizeBackupConfig({
    maxPreRestoreBackups: 0.5,
    retentionDays: 1.9,
  });
  assertEquals(config.maxPreRestoreBackups, 1, "0.5 floors to 0, clamps to 1");
  assertEquals(config.retentionDays, 1, "fractional days floored");
  assertEquals(
    normalizeBackupConfig({ retentionDays: 2.9 }).retentionDays,
    2,
    "2.9 floors to 2",
  );
}

function testParseConfigInvalidJson(): void {
  const parsed = parseBackupConfig("{not json");
  assertEquals(parsed.enabled, true, "falls back to defaults");
  assertEquals(parsed.intervalMinutes, 15, "interval default");
}

/* ---------------- round 4 additions ---------------- */

const OK_URLS = [
  "http://example.com/",
  "https://example.com/a?b=c#d",
  "about:blank",
  "about:newtab",
  "about:home",
  "about:reader?url=https%3A%2F%2Fexample.com%2F",
  "moz-extension://abc/page.html",
];
const BAD_URLS = [
  "javascript:alert(1)",
  "data:text/html,<b>x</b>",
  "chrome://browser/content/browser.xhtml",
  "resource://gre/modules/x.sys.mjs",
  "file:///etc/passwd",
  "about:config",
  "about:preferences",
  "view-source:https://example.com/",
  "about:reader?url=javascript%3Aalert(1)",
  "about:reader",
  "not a url",
  "",
  "ftp://example.com/",
];

function testIsRestorableTabUrl(): void {
  for (const url of OK_URLS) {
    assert(isRestorableTabUrl(url), `allows ${url}`);
  }
  for (const url of BAD_URLS) {
    assert(!isRestorableTabUrl(url), `rejects ${url}`);
  }
}

function testSanitizeAllowlist(): void {
  const ICON = "data:image/png;base64,iVBORw0KGgo=";
  const input = {
    entries: [
      {
        url: "https://example.com/1",
        title: "One",
        charset: "UTF-8",
        ID: 7,
        docshellUUID: "{abc}",
        originalURI: "https://example.com/orig",
        resultPrincipalURI: "https://example.com/res",
        hasUserInteraction: true,
        persist: true,
        cacheKey: 3,
        postdata_b64: "c2VjcmV0",
        referrerInfo: "x",
        triggeringPrincipal_base64: "t",
        principalToInherit_base64: "p",
        partitionedPrincipalToInherit_base64: "pp",
        csp: "c",
        structuredCloneState: "s",
        presState: { a: 1 },
        children: [
          {
            url: "https://example.com/child",
            postdata_b64: "zzz",
            triggeringPrincipal_base64: "t",
            children: [{ url: "javascript:1" }],
          },
          { url: "data:text/html,x" },
        ],
      },
    ],
    index: 1,
    requestedIndex: 1,
    userContextId: 2,
    pinned: true,
    hidden: false,
    lastAccessed: 123,
    image: "https://example.com/i.png",
    userTypedValue: "typed secret",
    userTypedClear: 1,
    extData: { k: "v" },
    unknownKey: 1,
    attributes: {
      image: "x",
      floorpWorkspaceId: WS_A,
      nested: { a: 1 },
    },
  };
  const before = JSON.stringify(input);
  const out = sanitizeTabState(input)!;
  assertEquals(JSON.stringify(input), before, "input not mutated");
  for (
    const key of ["userTypedValue", "userTypedClear", "extData", "unknownKey"]
  ) {
    assert(!(key in out), `${key} dropped`);
  }
  assertEquals(out.userContextId, 2, "userContextId kept");
  assertEquals(out.pinned, true, "pinned kept");
  assert(!("image" in out), "remote image dropped");
  assertEquals(
    sanitizeTabState({ entries: [{ url: "https://a.example/" }], image: ICON })
      ?.image,
    ICON,
    "inline data: image kept",
  );
  assert(
    !("image" in
      sanitizeTabState({
        entries: [{ url: "https://a.example/" }],
        image: "data:text/html,<script>1</script>",
      })!),
    "non-image data: URL dropped",
  );
  assertEquals(
    sanitizeTabState({ pinned: true }),
    null,
    "state without entries rejected",
  );
  const attrs = out.attributes as Record<string, unknown>;
  assert(!("floorpWorkspaceId" in attrs), "marker dropped");
  assert(!("nested" in attrs), "non-primitive attribute dropped");
  const entry = (out.entries as Array<Record<string, unknown>>)[0];
  for (
    const key of [
      "postdata_b64",
      "referrerInfo",
      "triggeringPrincipal_base64",
      "principalToInherit_base64",
      "partitionedPrincipalToInherit_base64",
      "csp",
      "structuredCloneState",
      "presState",
    ]
  ) {
    assert(!(key in entry), `entry ${key} dropped`);
  }
  assertEquals(entry.title, "One", "title kept");
  assertEquals(entry.cacheKey, 3, "cacheKey kept");
  assertEquals(entry.ID, 7, "numeric ID kept");
  for (const key of ["originalURI", "resultPrincipalURI", "docshellUUID"]) {
    assert(!(key in entry), `entry ${key} dropped (principal spoofing)`);
  }
  const children = entry.children as Array<Record<string, unknown>>;
  assertEquals(children.length, 1, "bad child dropped");
  assert(!("postdata_b64" in children[0]), "child postdata dropped");
  assert(
    !("triggeringPrincipal_base64" in children[0]),
    "child principal dropped",
  );
  assert(!("children" in children[0]), "grandchild with bad url dropped");
}

function testSanitizeDropsBadEntriesAndClampsIndex(): void {
  const out = sanitizeTabState({
    entries: [
      { url: "https://a.example/" },
      { url: "javascript:alert(1)" },
      { url: "https://c.example/" },
    ],
    index: 2,
  })!;
  assertEquals((out.entries as unknown[]).length, 2, "bad entry dropped");
  assertEquals(out.index, 1, "index falls back to previous surviving entry");

  const shifted = sanitizeTabState({
    entries: [{ url: "data:x" }, { url: "https://b.example/" }],
    index: 2,
  })!;
  assertEquals(shifted.index, 1, "index shifted after leading drop");

  const none = sanitizeTabState({
    entries: [{ url: "chrome://x/" }],
    index: 1,
  });
  assertEquals(none, null, "no restorable entries -> null");

  const high = sanitizeTabState({
    entries: [{ url: "https://a.example/" }],
    index: 99,
  })!;
  assertEquals(high.index, 1, "out-of-range index clamped");
}

function testSanitizeMultibyteInputAndDepth(): void {
  let deep: Record<string, unknown> = { url: "https://deep.example/" };
  for (let i = 0; i < 20; i++) {
    deep = { url: "https://deep.example/", children: [deep] };
  }
  const out = sanitizeTabState({ entries: [deep], index: 1 })!;
  assert(out !== null, "deep state survives");
  let depth = 0;
  let node = (out.entries as Array<Record<string, unknown>>)[0];
  while (node.children) {
    node = (node.children as Array<Record<string, unknown>>)[0];
    depth++;
  }
  assert(depth <= 8, "children depth bounded");
}

function testCapturedAtValidation(): void {
  for (
    const bad of [
      -1,
      1.5,
      Number.NaN,
      Number.POSITIVE_INFINITY,
      Number.MAX_SAFE_INTEGER + 2,
      MAX_CAPTURED_AT + 1,
      "1700000000000",
    ]
  ) {
    assert(
      !isWorkspaceBackupFile({ ...makeFile(), capturedAt: bad }),
      `rejects capturedAt ${String(bad)}`,
    );
  }
  assert(
    isWorkspaceBackupFile(makeFile({ capturedAt: 0 })),
    "0 is accepted",
  );
  assert(
    isWorkspaceBackupFile(makeFile({ capturedAt: MAX_CAPTURED_AT })),
    "max accepted",
  );
}

function testParseFileNameRejectsOutOfRange(): void {
  for (
    const name of [
      "backup-20261301-000000-abcdef01.jsonlz4",
      "backup-20260001-000000-abcdef01.jsonlz4",
      "backup-20261132-000000-abcdef01.jsonlz4",
      "backup-20260230-000000-abcdef01.jsonlz4",
      "backup-20261117-240000-abcdef01.jsonlz4",
      "backup-20261117-006000-abcdef01.jsonlz4",
      "backup-20261117-000060-abcdef01.jsonlz4",
      "backup-00501117-000000-abcdef01.jsonlz4",
    ]
  ) {
    assertEquals(parseBackupFileName(name), null, `rejects ${name}`);
  }
  assert(
    parseBackupFileName("backup-20240229-235959-abcdef01.jsonlz4") !== null,
    "leap day ok",
  );
}

function testDeterministicTieBreak(): void {
  const a = makeSummary("a.jsonlz4", "auto", 5);
  const b = makeSummary("b.jsonlz4", "auto", 5);
  const forward = sortBackupsNewestFirst([a, b]).map((x) => x.fileName);
  const reverse = sortBackupsNewestFirst([b, a]).map((x) => x.fileName);
  assertEquals(forward.join(","), reverse.join(","), "order independent");
  assertEquals(forward[0], "b.jsonlz4", "fileName desc is secondary key");

  const p1 = makeSummary("p1.jsonlz4", "pre-restore", 5);
  const p2 = makeSummary("p2.jsonlz4", "pre-restore", 5);
  const e1 = selectExcessPreRestoreBackups([p1, p2], 1);
  const e2 = selectExcessPreRestoreBackups([p2, p1], 1);
  assertEquals(e1[0].fileName, e2[0].fileName, "excess deterministic");
  assertEquals(e1[0].fileName, "p1.jsonlz4", "lower name is older on tie");
}

function testMinKeep(): void {
  const now = 1700000000000;
  const old = (n: number, kind: "auto" | "manual" = "auto") =>
    makeSummary(`o${n}.jsonlz4`, kind, now - (100 + n) * DAY);
  const summaries = [old(1), old(2), old(3, "manual"), old(4), old(5)];
  const expired = selectExpiredBackups(summaries, 7, now);
  assertEquals(MIN_KEEP_BACKUPS, 3, "default minKeep");
  assertEquals(expired.length, 2, "newest 3 survive regardless of age");
  assertEquals(
    expired.map((x) => x.fileName).sort().join(","),
    "o4.jsonlz4,o5.jsonlz4",
    "oldest two expire",
  );
  assertEquals(
    selectExpiredBackups(summaries, 7, now, 5).length,
    0,
    "minKeep 5 keeps all",
  );
  const withPre = [
    ...summaries.slice(0, 3),
    makeSummary("pre.jsonlz4", "pre-restore", now - 500 * DAY),
  ];
  assertEquals(
    selectExpiredBackups(withPre, 7, now).length,
    0,
    "pre-restore neither expired nor counted toward minKeep",
  );

  // Future-dated backups (clock rollback / crafted files) never occupy the
  // kept slots; they expire once enough plausibly dated backups exist.
  const future = (name: string) => makeSummary(name, "auto", now + 400 * DAY);
  const mixed = [future("f1.jsonlz4"), old(1), old(2), old(3)];
  assertEquals(
    selectExpiredBackups(mixed, 7, now).map((x) => x.fileName).join(","),
    "f1.jsonlz4",
    "future-dated backup expires and the 3 plausible ones are kept",
  );
  // After the clock jumps back, the whole real history looks "future": it must
  // survive until fresh backups exist.
  assertEquals(
    selectExpiredBackups(
      [
        future("f1.jsonlz4"),
        future("f2.jsonlz4"),
        future("f3.jsonlz4"),
        old(1),
      ],
      7,
      now,
    ).length,
    0,
    "clock rollback does not wipe the history",
  );
  // Pre-restore: future-dated copies rank last and go first.
  const pre = (name: string, at: number) =>
    makeSummary(name, "pre-restore", at);
  const excess = selectExcessPreRestoreBackups(
    [pre("future.jsonlz4", now + 400 * DAY), pre("fresh.jsonlz4", now)],
    1,
    now,
  );
  assertEquals(
    excess.map((x) => x.fileName).join(","),
    "future.jsonlz4",
    "future-dated pre-restore cannot crowd out a fresh safety copy",
  );
  assertEquals(
    selectExpiredBackups(
      [makeSummary("skew.jsonlz4", "auto", now + 60 * 60 * 1000)],
      7,
      now,
    ).length,
    0,
    "small clock skew is tolerated",
  );
}

function testConfigClamps(): void {
  const hi = normalizeBackupConfig({
    intervalMinutes: 1e9,
    retentionDays: 1e9,
    maxPreRestoreBackups: 1e9,
  });
  assertEquals(hi.intervalMinutes, 1440, "interval max");
  assertEquals(hi.retentionDays, 365, "retention max");
  assertEquals(hi.maxPreRestoreBackups, 50, "pre-restore max");

  const zero = normalizeBackupConfig({
    intervalMinutes: 0,
    retentionDays: 0,
    maxPreRestoreBackups: 0,
  });
  assertEquals(zero.intervalMinutes, 1, "interval min");
  assertEquals(zero.retentionDays, 1, "retention min");
  assertEquals(zero.maxPreRestoreBackups, 1, "pre-restore min");

  const frac = normalizeBackupConfig({
    intervalMinutes: 1.5,
    retentionDays: 1.5,
    maxPreRestoreBackups: 1.5,
  });
  assertEquals(frac.intervalMinutes, 1, "1.5 -> 1");
  assertEquals(frac.retentionDays, 1, "1.5 -> 1");
  assertEquals(frac.maxPreRestoreBackups, 1, "1.5 -> 1");

  for (const bad of [Number.POSITIVE_INFINITY, Number.NaN]) {
    const c = normalizeBackupConfig({
      intervalMinutes: bad,
      retentionDays: bad,
      maxPreRestoreBackups: bad,
    });
    assertEquals(c.intervalMinutes, 15, `interval ${bad} default`);
    assertEquals(c.retentionDays, 7, `retention ${bad} default`);
    assertEquals(c.maxPreRestoreBackups, 10, `pre-restore ${bad} default`);
  }
  assertEquals(
    normalizeBackupConfig({ intervalMinutes: -5 }).intervalMinutes,
    1,
    "negative clamps to min",
  );
}

function testLimitConstants(): void {
  assertEquals(WORKSPACE_BACKUP_LIMITS.maxNameLength, 256, "name");
  assertEquals(WORKSPACE_BACKUP_LIMITS.maxIconLength, 2048, "icon");
  assertEquals(
    WORKSPACE_BACKUP_LIMITS.maxDecompressedBytes,
    256 * 1024 * 1024,
    "decompressed",
  );
}

function testLimitBoundaries(): void {
  const L = WORKSPACE_BACKUP_LIMITS;
  const windowsOf = (n: number) =>
    Array.from({ length: n }, (_, i) => makeWindow(1, i === 0));
  assert(
    isWorkspaceBackupFile(makeFile({ windows: windowsOf(L.maxWindows) })),
    "maxWindows ok",
  );
  assert(
    !isWorkspaceBackupFile(makeFile({ windows: windowsOf(L.maxWindows + 1) })),
    "maxWindows+1 rejected",
  );

  const bigTabs = (n: number, overrides?: Partial<WorkspaceBackupTab>) => ({
    isFocused: true,
    tabs: Array.from({ length: n }, () => makeTab(1, overrides)),
  });
  assert(
    isWorkspaceBackupFile(makeFile({ windows: [bigTabs(L.maxTabsPerWindow)] })),
    "maxTabsPerWindow ok",
  );
  assert(
    !isWorkspaceBackupFile(
      makeFile({ windows: [bigTabs(L.maxTabsPerWindow + 1)] }),
    ),
    "maxTabsPerWindow+1 rejected",
  );

  const one = (overrides: Partial<WorkspaceBackupTab>) =>
    makeFile({ windows: [{ isFocused: true, tabs: [makeTab(0, overrides)] }] });
  assert(
    isWorkspaceBackupFile(one({ title: "a".repeat(L.maxTitleLength) })),
    "title at limit ok",
  );
  assert(
    !isWorkspaceBackupFile(one({ title: "a".repeat(L.maxTitleLength + 1) })),
    "title+1 rejected",
  );
  const urlAt = "https://e.example/" + "a".repeat(L.maxUrlLength - 18);
  assertEquals(urlAt.length, L.maxUrlLength, "url length sanity");
  assert(isWorkspaceBackupFile(one({ url: urlAt })), "url at limit ok");
  assert(!isWorkspaceBackupFile(one({ url: urlAt + "a" })), "url+1 rejected");

  // Byte budget: "あ" is 3 bytes in UTF-8 but 1 UTF-16 unit.
  const overhead = JSON.stringify({ k: "" }).length;
  const fit = Math.floor((L.maxTabStateBytes - overhead) / 3);
  assert(
    isWorkspaceBackupFile(one({ state: { k: "あ".repeat(fit) } })),
    "multibyte state within bytes ok",
  );
  assert(
    !isWorkspaceBackupFile(one({ state: { k: "あ".repeat(fit + 1) } })),
    "multibyte state over bytes rejected (code units would pass)",
  );
  assert(
    "あ".repeat(fit + 1).length < L.maxTabStateBytes,
    "sanity: code unit count is below the byte limit",
  );

  // maxTotalTabs: 6 windows x 1000 = 6000 > 5000 though each window is ok.
  assert(
    !isWorkspaceBackupFile(
      makeFile({ windows: Array.from({ length: 6 }, () => bigTabs(1000)) }),
    ),
    "maxTotalTabs exceeded rejected",
  );
}

function testIntegrityFailures(): void {
  const state = (
    over: Partial<WorkspaceBackupState>,
  ): WorkspaceBackupState => ({
    ...makeWorkspaceState(),
    ...over,
  });
  const wsOf = (id: string, name = "N") => ({
    workspaceId: id as TWorkspaceID,
    name,
    userContextId: 0,
  });
  const cases: Array<[string, WorkspaceBackupState]> = [
    [
      "duplicate workspaceId",
      state({ workspaces: [wsOf(WS_A), wsOf(WS_A)], order: [WS_A, WS_A] }),
    ],
    ["order missing an id", state({ order: [WS_A] })],
    ["order duplicate", state({ order: [WS_A, WS_A] })],
    [
      "order unknown id",
      state({
        order: [WS_A, "00000000-0000-4000-8000-0000000000ff" as TWorkspaceID],
      }),
    ],
    [
      "defaultID unknown",
      state({
        defaultID: "00000000-0000-4000-8000-0000000000ff" as TWorkspaceID,
      }),
    ],
    [
      "currentWorkspaceId unknown",
      state({
        currentWorkspaceId:
          "00000000-0000-4000-8000-0000000000ff" as TWorkspaceID,
      }),
    ],
    [
      "non-uuid workspaceId",
      state({
        workspaces: [wsOf("abc"), wsOf(WS_B)],
        order: ["abc" as TWorkspaceID, WS_B],
      }),
    ],
    [
      "name too long",
      state({
        workspaces: [wsOf(WS_A, "n".repeat(257)), wsOf(WS_B)],
      }),
    ],
    [
      "icon too long",
      state({
        workspaces: [
          { ...wsOf(WS_A), icon: "i".repeat(2049) },
          wsOf(WS_B),
        ],
      }),
    ],
  ];
  for (const [label, workspaceState] of cases) {
    assert(
      !isWorkspaceBackupFile(makeFile({ workspaceState })),
      `rejects ${label}`,
    );
  }
  assert(
    isWorkspaceBackupFile(makeFile({
      workspaceState: state({
        workspaces: [wsOf(WS_A, "n".repeat(256)), {
          ...wsOf(WS_B),
          icon: "i".repeat(2048),
        }],
      }),
    })),
    "name/icon at limit ok",
  );
  assert(
    isWorkspaceBackupFile(makeFile({
      workspaceState: state({ defaultID: null, currentWorkspaceId: null }),
    })),
    "null default/current ok",
  );

  // order longer than maxWorkspaces
  const many = Array.from(
    { length: 201 },
    (_, i) => `00000000-0000-4000-8000-${String(i).padStart(12, "0")}`,
  );
  assert(
    !isWorkspaceBackupFile(makeFile({
      workspaceState: {
        workspaces: many.map((id) => wsOf(id)),
        order: many as TWorkspaceID[],
        defaultID: null,
        currentWorkspaceId: null,
      },
    })),
    "maxWorkspaces+1 rejected",
  );

  // tabs mode tabs must not carry workspaceId
  assert(
    !isWorkspaceBackupFile(makeFile({
      mode: "tabs",
      workspaceState: undefined,
      windows: [{
        isFocused: true,
        tabs: [makeTab(0, { workspaceId: WS_A })],
      }],
    })),
    "tabs mode tab with workspaceId rejected",
  );
  // malformed tab workspaceId
  assert(
    !isWorkspaceBackupFile(makeFile({
      windows: [{
        isFocused: true,
        tabs: [makeTab(0, { workspaceId: "nope" as TWorkspaceID })],
      }],
    })),
    "bad tab workspaceId format rejected",
  );
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
    {
      name: "fractional retention cap is floored",
      fn: testFractionalRetentionCapIsFloored,
    },
    { name: "parse config invalid json", fn: testParseConfigInvalidJson },
    { name: "isRestorableTabUrl", fn: testIsRestorableTabUrl },
    { name: "sanitize allowlist", fn: testSanitizeAllowlist },
    {
      name: "sanitize drops bad entries and clamps index",
      fn: testSanitizeDropsBadEntriesAndClampsIndex,
    },
    { name: "sanitize depth bound", fn: testSanitizeMultibyteInputAndDepth },
    { name: "capturedAt validation", fn: testCapturedAtValidation },
    {
      name: "parse file name rejects out-of-range",
      fn: testParseFileNameRejectsOutOfRange,
    },
    { name: "deterministic tie-break", fn: testDeterministicTieBreak },
    { name: "minKeep", fn: testMinKeep },
    { name: "config clamps", fn: testConfigClamps },
    { name: "limit constants", fn: testLimitConstants },
    { name: "limit boundaries", fn: testLimitBoundaries },
    { name: "integrity failures", fn: testIntegrityFailures },
  ];

  console.log(`workspacesBackupHelpers: running ${tests.length} tests`);
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
