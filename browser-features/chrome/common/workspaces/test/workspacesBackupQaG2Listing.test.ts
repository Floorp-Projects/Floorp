// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA g2 (E-01..E-20): listing, loading and tamper resistance of backup files.

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { withWindows } from "./workspace-last-tab-test-utils.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
} from "../utils/workspaces-static-names.ts";
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { WorkspaceBackupSummary } from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
};

type Json = Record<string, unknown>;

interface ConsoleEvent {
  level?: string;
  arguments?: unknown[];
}

const BACKUP_DIR = PathUtils.join(
  PathUtils.profileDir,
  "workspaces",
  "backup",
);
const WS_A = "00000000-0000-4000-8000-0000000000a1";
const WS_B = "00000000-0000-4000-8000-0000000000b2";
const MINUTE = 60 * 1000;

function hostWindow(): BackupWindow {
  return globalThis as unknown as BackupWindow;
}

function hostFuncs(): WorkspacesBackupFuncs {
  const funcs = hostWindow().workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  return funcs;
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function dirPath(name: string): string {
  return PathUtils.join(BACKUP_DIR, name);
}

async function listDir(): Promise<string[]> {
  if (!(await IOUtils.exists(BACKUP_DIR))) return [];
  return (await IOUtils.getChildren(BACKUP_DIR)).map((path) =>
    PathUtils.filename(path)
  );
}

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let index = 0; index < a.length; index++) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

function savePref(name: string): () => void {
  const had = Services.prefs.prefHasUserValue(name);
  const value = had ? Services.prefs.getStringPref(name) : "";
  return () => {
    if (had) Services.prefs.setStringPref(name, value);
    else if (Services.prefs.prefHasUserValue(name)) {
      Services.prefs.clearUserPref(name);
    }
  };
}

/** Restores the backup folder and backup prefs to their pre-test state. */
async function withBackupDirSnapshot(
  check: () => Promise<void>,
): Promise<void> {
  const existed = await IOUtils.exists(BACKUP_DIR);
  const snapshot = new Map<string, Uint8Array>();
  for (const name of await listDir()) {
    const info = await IOUtils.stat(dirPath(name));
    if (info.type === "regular") {
      snapshot.set(name, await IOUtils.read(dirPath(name)));
    }
  }
  const restoreConfig = savePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME);
  const restoreLastRun = savePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME);
  try {
    await check();
  } finally {
    restoreConfig();
    restoreLastRun();
    await sleep(300);
    for (const name of await listDir()) {
      if (!snapshot.has(name)) {
        await IOUtils.remove(dirPath(name), { recursive: true });
      }
    }
    if (existed || snapshot.size > 0) {
      await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
    } else if (await IOUtils.exists(BACKUP_DIR)) {
      await IOUtils.remove(BACKUP_DIR, { recursive: true });
    }
    for (const [name, bytes] of snapshot) {
      const path = dirPath(name);
      const current = (await IOUtils.exists(path))
        ? await IOUtils.read(path)
        : null;
      if (!current || !sameBytes(current, bytes)) {
        await IOUtils.write(path, bytes);
      }
    }
  }
}

let idCounter = 0;
function nextShortId(): string {
  idCounter++;
  return `e2${(Date.now() % 0xffff).toString(16).padStart(4, "0")}${
    (idCounter % 0xff).toString(16).padStart(2, "0")
  }`;
}

function makeTab(workspaceId?: string, overrides: Json = {}): Json {
  const tab: Json = {
    state: null,
    title: "QA g2",
    url: "https://example.com/qa-g2",
    pinned: false,
    isSelected: true,
    userContextId: 0,
    lastShownWorkspaceId: null,
  };
  if (workspaceId) tab.workspaceId = workspaceId;
  return { ...tab, ...overrides };
}

function workspacesFile(overrides: Json = {}): Json {
  return {
    version: 2,
    kind: "manual",
    mode: "workspaces",
    capturedAt: Date.now() - MINUTE,
    windows: [{ isFocused: true, tabs: [makeTab(WS_A), makeTab(WS_B)] }],
    workspaceState: {
      workspaces: [
        { workspaceId: WS_A, name: "A", userContextId: 0, icon: null },
        { workspaceId: WS_B, name: "B", userContextId: 1, icon: null },
      ],
      order: [WS_A, WS_B],
      defaultID: WS_A,
      currentWorkspaceId: WS_A,
    },
    ...overrides,
  };
}

function tabsFile(overrides: Json = {}): Json {
  return {
    version: 2,
    kind: "auto",
    mode: "tabs",
    capturedAt: Date.now() - MINUTE,
    windows: [
      { isFocused: true, tabs: [makeTab()] },
      { isFocused: false, tabs: [makeTab(), makeTab()] },
    ],
    ...overrides,
  };
}

async function writeCompressed(name: string, value: unknown): Promise<void> {
  await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
  const text = typeof value === "string" ? value : JSON.stringify(value);
  await IOUtils.write(dirPath(name), new TextEncoder().encode(text), {
    compress: true,
  });
}

async function writeBackup(
  value: Json,
  shortId = nextShortId(),
): Promise<string> {
  const capturedAt = typeof value.capturedAt === "number" &&
      Number.isFinite(value.capturedAt) && value.capturedAt >= 0 &&
      value.capturedAt < 253402300799000
    ? value.capturedAt
    : Date.now();
  const name = formatBackupFileName(Math.floor(capturedAt), shortId);
  await writeCompressed(name, value);
  return name;
}

async function listedNames(): Promise<string[]> {
  return (await hostFuncs().listBackups()).map((entry) => entry.fileName);
}

function consoleErrors(): ConsoleEvent[] {
  const storage =
    (Cc as Record<string, { getService(iface: unknown): unknown }>)[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage) as {
      getEvents(): ConsoleEvent[];
    };
  return storage.getEvents().filter((event) => event.level === "error");
}

/**
 * The tampered file must be hidden from the list, a valid control file must
 * still be listed, and restoring the tampered file must fail before touching
 * any tab.
 */
async function assertRejected(label: string, name: string): Promise<void> {
  const control = await writeBackup(tabsFile());
  const names = await listedNames();
  assert(!names.includes(name), `${label}: ${name} is not listed`);
  assert(names.includes(control), `${label}: the valid file is still listed`);
  const tabsBefore = hostWindow().gBrowser.tabs.length;
  const result = await hostFuncs().restoreBackup(name);
  assert(!result.success, `${label}: restore is refused`);
  assertEquals(
    hostWindow().gBrowser.tabs.length,
    tabsBefore,
    `${label}: no tab was opened or closed`,
  );
}

async function assertTamperedRejected(label: string, value: Json) {
  await assertRejected(label, await writeBackup(value));
}

// E-01
async function testNewestFirst(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const base = Date.now() - 60 * MINUTE;
    // The stamp in the name disagrees with capturedAt for "old": capturedAt
    // is authoritative.
    const old = formatBackupFileName(base + 50 * MINUTE, "e1000001");
    await writeCompressed(old, tabsFile({ capturedAt: base }));
    const middleLow = await writeBackup(
      tabsFile({ capturedAt: base + 10 * MINUTE }),
      "e1000002",
    );
    const middleHigh = await writeBackup(
      tabsFile({ capturedAt: base + 10 * MINUTE }),
      "e10000ff",
    );
    const newest = await writeBackup(
      workspacesFile({ capturedAt: base + 20 * MINUTE }),
      "e1000003",
    );
    const ours = new Set([old, middleLow, middleHigh, newest]);
    const order = (await listedNames()).filter((name) => ours.has(name));
    assertEquals(
      order.join(","),
      [newest, middleHigh, middleLow, old].join(","),
      "capturedAt descending, ties by file name descending",
    );
    const all = await hostFuncs().listBackups();
    for (let index = 1; index < all.length; index++) {
      assert(
        all[index - 1].capturedAt >= all[index].capturedAt,
        "whole list is sorted newest first",
      );
    }
  });
}

// E-02
async function testSummaryFields(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const cases: [Json, Partial<WorkspaceBackupSummary>][] = [
      [workspacesFile({ kind: "auto" }), {
        kind: "auto",
        mode: "workspaces",
        workspaceCount: 2,
        windowCount: 1,
        tabCount: 2,
      }],
      [workspacesFile({ kind: "manual" }), { kind: "manual" }],
      [workspacesFile({ kind: "pre-restore" }), { kind: "pre-restore" }],
      [tabsFile({ kind: "manual" }), {
        kind: "manual",
        mode: "tabs",
        workspaceCount: 0,
        windowCount: 2,
        tabCount: 3,
      }],
    ];
    for (const [file, expected] of cases) {
      const name = await writeBackup(file);
      const summary = (await hostFuncs().listBackups()).find((entry) =>
        entry.fileName === name
      );
      assert(summary, `${name} is listed`);
      assertEquals(summary.id, name, "id is the file name");
      assertEquals(summary.capturedAt, file.capturedAt, "capturedAt");
      assertEquals(
        summary.fileSize,
        (await IOUtils.stat(dirPath(name))).size,
        "fileSize is the on-disk size",
      );
      for (const [key, value] of Object.entries(expected)) {
        assertEquals(
          summary[key as keyof WorkspaceBackupSummary],
          value,
          `${name}: ${key}`,
        );
      }
    }
  });
}

// E-03
async function testRandomBytes(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    for (const size of [0, 5, 12, 4096]) {
      const bytes = new Uint8Array(size);
      crypto.getRandomValues(bytes);
      const name = formatBackupFileName(Date.now(), nextShortId());
      await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
      await IOUtils.write(dirPath(name), bytes);
      await assertRejected(`random ${size} bytes`, name);
    }
    // Correct magic followed by garbage.
    const header = new Uint8Array([
      0x6d,
      0x6f,
      0x7a,
      0x4c,
      0x7a,
      0x34,
      0x30,
      0x00,
      0x00,
      0x01,
      0x00,
      0x00,
    ]);
    const body = new Uint8Array(256);
    crypto.getRandomValues(body);
    const mixed = new Uint8Array(header.length + body.length);
    mixed.set(header);
    mixed.set(body, header.length);
    const name = formatBackupFileName(Date.now(), nextShortId());
    await IOUtils.write(dirPath(name), mixed);
    await assertRejected("magic + random body", name);
  });
}

// E-04
async function testBrokenJson(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    for (
      const text of [
        "{",
        '{"version":2,"kind":"manual"',
        "not json",
        "",
        "null",
        "[]",
        '"string"',
      ]
    ) {
      const name = formatBackupFileName(Date.now(), nextShortId());
      await writeCompressed(name, text);
      await assertRejected(`json ${JSON.stringify(text)}`, name);
    }
  });
}

// E-05
async function testVersionOne(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected("v1", workspacesFile({ version: 1 }));
    await assertTamperedRejected("v1 tabs", tabsFile({ version: 1 }));
  });
}

// E-06
async function testInvalidVersion(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const missing = workspacesFile();
    delete missing.version;
    await assertTamperedRejected("version missing", missing);
    await assertTamperedRejected("version 3", workspacesFile({ version: 3 }));
    await assertTamperedRejected(
      'version "2"',
      workspacesFile({ version: "2" }),
    );
    await assertTamperedRejected(
      "version 2.5",
      workspacesFile({ version: 2.5 }),
    );
  });
}

// E-07
async function testTabsModeWithWorkspaceState(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "tabs + workspaceState",
      tabsFile({ workspaceState: workspacesFile().workspaceState }),
    );
    await assertTamperedRejected(
      "tabs + workspaceState null",
      tabsFile({ workspaceState: null }),
    );
  });
}

// E-08
async function testTabsModeWithTabWorkspaceId(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "tabs + tab.workspaceId",
      tabsFile({
        windows: [{ isFocused: true, tabs: [makeTab(), makeTab(WS_A)] }],
      }),
    );
  });
}

function withState(state: Json): Json {
  return workspacesFile({
    workspaceState: {
      ...(workspacesFile().workspaceState as Json),
      ...state,
    },
  });
}

// E-09
async function testOrderNotPermutation(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "order missing id",
      withState({
        order: [WS_A],
      }),
    );
    await assertTamperedRejected(
      "order duplicate",
      withState({
        order: [WS_A, WS_A],
      }),
    );
    await assertTamperedRejected(
      "order unknown id",
      withState({
        order: [WS_A, "00000000-0000-4000-8000-0000000000c3"],
      }),
    );
    await assertTamperedRejected(
      "order extra id",
      withState({
        order: [WS_A, WS_B, "00000000-0000-4000-8000-0000000000c3"],
      }),
    );
    await assertTamperedRejected(
      "order not array",
      withState({
        order: `${WS_A},${WS_B}`,
      }),
    );
  });
}

// E-10
async function testDefaultIdUnknown(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "defaultID unknown",
      withState({
        defaultID: "00000000-0000-4000-8000-0000000000c3",
      }),
    );
    await assertTamperedRejected(
      "defaultID not uuid",
      withState({
        defaultID: "default",
      }),
    );
    await assertTamperedRejected(
      "currentWorkspaceId unknown",
      withState({
        currentWorkspaceId: "00000000-0000-4000-8000-0000000000c3",
      }),
    );
  });
}

// E-11
async function testDuplicateWorkspaceIds(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "duplicate workspace ids",
      withState({
        workspaces: [
          { workspaceId: WS_A, name: "A", userContextId: 0, icon: null },
          { workspaceId: WS_A, name: "A2", userContextId: 0, icon: null },
        ],
        order: [WS_A, WS_A],
      }),
    );
    await assertTamperedRejected(
      "duplicate ids, deduped order",
      withState({
        workspaces: [
          { workspaceId: WS_A, name: "A", userContextId: 0, icon: null },
          { workspaceId: WS_A, name: "A2", userContextId: 0, icon: null },
        ],
        order: [WS_A],
      }),
    );
  });
}

// E-12
async function testCapturedAtOutOfRange(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    for (
      const capturedAt of [
        -1,
        1.5,
        Date.now() + 0.5,
        Date.UTC(9999, 11, 31) + 1,
        Date.UTC(10000, 0, 1),
        "1700000000000",
        null,
      ]
    ) {
      await assertTamperedRejected(
        `capturedAt ${String(capturedAt)}`,
        workspacesFile({ capturedAt }),
      );
    }
  });
}

// E-13
async function testEmptyWindows(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "windows []",
      workspacesFile({ windows: [] }),
    );
    await assertTamperedRejected("tabs windows []", tabsFile({ windows: [] }));
    await assertTamperedRejected(
      "windows missing",
      tabsFile({ windows: undefined }),
    );
  });
}

// E-14
async function testNegativeUserContextId(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await assertTamperedRejected(
      "tab userContextId -1",
      tabsFile({
        windows: [{
          isFocused: true,
          tabs: [makeTab(undefined, { userContextId: -1 })],
        }],
      }),
    );
    await assertTamperedRejected(
      "workspace userContextId -1",
      withState({
        workspaces: [
          { workspaceId: WS_A, name: "A", userContextId: -1, icon: null },
          { workspaceId: WS_B, name: "B", userContextId: 0, icon: null },
        ],
      }),
    );
  });
}

// E-15
async function testOversizedHeader(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    for (const declared of [256 * 1024 * 1024 + 1, 0xffffffff]) {
      const header = new Uint8Array([
        0x6d,
        0x6f,
        0x7a,
        0x4c,
        0x7a,
        0x34,
        0x30,
        0x00,
        declared & 0xff,
        (declared >>> 8) & 0xff,
        (declared >>> 16) & 0xff,
        (declared >>> 24) & 0xff,
      ]);
      const bytes = new Uint8Array(header.length + 64);
      bytes.set(header);
      const name = formatBackupFileName(Date.now(), nextShortId());
      await IOUtils.makeDirectory(BACKUP_DIR, { createAncestors: true });
      await IOUtils.write(dirPath(name), bytes);
      const before = consoleErrors().length;
      assert(
        !(await listedNames()).includes(name),
        `declared ${declared}: not listed`,
      );
      const result = await hostFuncs().restoreBackup(name);
      assert(!result.success, `declared ${declared}: restore refused`);
      assertEquals(
        result.success ? "" : result.error,
        "backup-too-large",
        `declared ${declared}: restore reports backup-too-large`,
      );
      const refusals = consoleErrors().slice(before).filter((event) =>
        String(event.arguments?.[0] ?? "").includes(
          "refusing to decompress oversized backup",
        )
      );
      assert(
        refusals.length >= 2,
        `declared ${declared}: rejected from the header before decompressing`,
      );
    }
  });
}

// E-16
async function testMalformedNames(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const valid = workspacesFile();
    const malformed = [
      "backup-2026131-120000-abcdef01.jsonlz4",
      "backup-202611241-120000-abcdef01.jsonlz4",
      "backup-20261124-12000-abcdef01.jsonlz4",
      "backup-20261124-1200000-abcdef01.jsonlz4",
      "backup-20261124-120000-abcdef0.jsonlz4",
      "backup-20261124-120000-abcdef012.jsonlz4",
      "backup-20261124-120000-abcdefgh.jsonlz4",
      "backup-20261124-120000-abcdef01.jsonlz4.bak",
      "backup-20261124-120000-abcdef01.json",
      "backup_20261124-120000-abcdef01.jsonlz4",
      " backup-20261124-120000-abcdef01.jsonlz4",
    ];
    for (const name of malformed) await writeCompressed(name, valid);
    const names = await listedNames();
    for (const name of malformed) {
      assert(!names.includes(name), `${JSON.stringify(name)} is ignored`);
    }
    // Case-insensitive by design (BACKUP_FILE_NAME_PATTERN has the i flag):
    // recorded so a behavior change is noticed.
    const upper = "BACKUP-20261124-120000-ABCDEF01.JSONLZ4";
    await writeCompressed(upper, valid);
    assert(
      (await listedNames()).includes(upper),
      "upper-case name is accepted (case-insensitive pattern)",
    );
  });
}

// E-17
async function testImpossibleTimestampNames(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const valid = workspacesFile();
    const impossible = [
      "backup-20261301-120000-abcdef11.jsonlz4",
      "backup-20261124-250000-abcdef12.jsonlz4",
      "backup-20260230-120000-abcdef13.jsonlz4",
      "backup-20261124-126000-abcdef14.jsonlz4",
      "backup-00000000-000000-abcdef15.jsonlz4",
    ];
    for (const name of impossible) await writeCompressed(name, valid);
    const names = await listedNames();
    const listed = impossible.filter((name) => names.includes(name));
    assertEquals(
      listed.join(","),
      "",
      "file names with impossible timestamps are ignored",
    );
  });
}

// E-18
async function testPathTraversal(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    const outsideName = formatBackupFileName(Date.now(), "e1800001");
    const outside = PathUtils.join(
      PathUtils.parent(BACKUP_DIR) ?? PathUtils.profileDir,
      outsideName,
    );
    await IOUtils.write(
      outside,
      new TextEncoder().encode(JSON.stringify(workspacesFile())),
      { compress: true },
    );
    try {
      const inside = await writeBackup(tabsFile());
      const hostile = [
        "../x",
        `../${outsideName}`,
        outside,
        `/${outsideName}`,
        `${inside}/../../${outsideName}`,
        `..\\${outsideName}`,
        `subdir/${inside}`,
        `${inside}\u0000`,
        "",
        "..",
        ".",
      ];
      for (const name of hostile) {
        const result = await hostFuncs().restoreBackup(name);
        assertEquals(
          result.success ? "success" : result.error,
          "invalid-file-name",
          `restore ${JSON.stringify(name)}`,
        );
        assertEquals(
          await hostFuncs().deleteBackup(name),
          false,
          `delete ${JSON.stringify(name)} is refused`,
        );
      }
      assert(await IOUtils.exists(outside), "file outside the folder is kept");
      assert(await IOUtils.exists(dirPath(inside)), "inside file is kept");
    } finally {
      await IOUtils.remove(outside, { ignoreAbsent: true });
    }
  });
}

// E-19
async function testMissingFolder(): Promise<void> {
  const saved = `${BACKUP_DIR}.qa-g2-saved-${Date.now()}`;
  const existed = await IOUtils.exists(BACKUP_DIR);
  if (existed) await IOUtils.move(BACKUP_DIR, saved);
  try {
    const list = await hostFuncs().listBackups();
    assertEquals(list.length, 0, "missing folder lists as empty");
    assertEquals(
      await hostFuncs().deleteAllBackups(),
      true,
      "delete all on a missing folder succeeds",
    );
    const result = await hostFuncs().restoreBackup(
      formatBackupFileName(Date.now(), "e1900001"),
    );
    assertEquals(
      result.success ? "success" : result.error,
      "backup-not-found",
      "restore on a missing folder reports not found",
    );
    assert(!(await IOUtils.exists(BACKUP_DIR)), "listing creates no folder");
  } finally {
    if (await IOUtils.exists(BACKUP_DIR)) {
      await IOUtils.remove(BACKUP_DIR, { recursive: true });
    }
    if (existed) await IOUtils.move(saved, BACKUP_DIR);
  }
}

// E-20
async function testDeleteWhileListing(): Promise<void> {
  await withBackupDirSnapshot(async () => {
    await withWindows(false, false, async (open) => {
      const other = (await open()) as BackupWindow;
      const otherFuncs = other.workspacesBackupFuncs;
      assert(otherFuncs, "second window exposes the backup API");
      for (let round = 0; round < 5; round++) {
        const created: string[] = [];
        for (let index = 0; index < 30; index++) {
          created.push(await writeBackup(tabsFile()));
        }
        const [list, deleted] = await Promise.all([
          hostFuncs().listBackups(),
          (async () => {
            await sleep(round * 5);
            return await otherFuncs.deleteAllBackups();
          })(),
        ]);
        assert(Array.isArray(list), `round ${round}: list resolves`);
        assert(deleted, `round ${round}: delete all succeeds`);
        const known = new Set(created);
        for (const entry of list) {
          assert(
            known.has(entry.fileName) || entry.fileName.startsWith("backup-"),
            `round ${round}: listed entries are real backups`,
          );
        }
        assertEquals(
          (await hostFuncs().listBackups()).length,
          0,
          `round ${round}: list is empty after delete all`,
        );
      }
      // Individual deletes racing a list, issued directly on disk.
      const created: string[] = [];
      for (let index = 0; index < 30; index++) {
        created.push(await writeBackup(tabsFile()));
      }
      const [list] = await Promise.all([
        hostFuncs().listBackups(),
        Promise.all(created.map((name) => otherFuncs.deleteBackup(name))),
      ]);
      assert(Array.isArray(list), "list resolves while files disappear");
    });
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG2Listing.test.ts", [
    { name: "E-01 newest first", fn: testNewestFirst },
    { name: "E-02 summary fields", fn: testSummaryFields },
    { name: "E-03 random bytes", fn: testRandomBytes },
    { name: "E-04 broken JSON", fn: testBrokenJson },
    { name: "E-05 version 1", fn: testVersionOne },
    { name: "E-06 missing or unknown version", fn: testInvalidVersion },
    {
      name: "E-07 tabs mode with workspaceState",
      fn: testTabsModeWithWorkspaceState,
    },
    {
      name: "E-08 tabs mode with tab workspaceId",
      fn: testTabsModeWithTabWorkspaceId,
    },
    { name: "E-09 order not a permutation", fn: testOrderNotPermutation },
    { name: "E-10 defaultID unknown", fn: testDefaultIdUnknown },
    { name: "E-11 duplicate workspace ids", fn: testDuplicateWorkspaceIds },
    { name: "E-12 capturedAt out of range", fn: testCapturedAtOutOfRange },
    { name: "E-13 empty windows", fn: testEmptyWindows },
    { name: "E-14 negative userContextId", fn: testNegativeUserContextId },
    { name: "E-15 oversized mozlz4 header", fn: testOversizedHeader },
    { name: "E-16 malformed file names", fn: testMalformedNames },
    {
      name: "E-17 impossible timestamp names",
      fn: testImpossibleTimestampNames,
    },
    { name: "E-18 path traversal", fn: testPathTraversal },
    { name: "E-19 missing folder", fn: testMissingFolder },
    { name: "E-20 delete while listing", fn: testDeleteWhileListing },
  ]);
}
