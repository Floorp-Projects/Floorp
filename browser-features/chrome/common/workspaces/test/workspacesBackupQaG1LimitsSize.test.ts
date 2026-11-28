// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup limit and truncation cases C-12 .. C-14.

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  settle,
  waitFor,
  withWindows,
} from "./workspace-last-tab-test-utils.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import { WORKSPACE_BACKUP_LIMITS } from "../utils/backup-types.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupTab,
} from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
  gBrowser: TestWindow["gBrowser"] & { pinTab(tab: XULElement): void };
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
}

interface StoreShape {
  defaultID: string;
  data: [string, Record<string, unknown>][];
  order: string[];
}

interface ConsoleEvent {
  level?: string;
  timeStamp?: number;
  arguments?: unknown[];
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };

const RUN = Date.now();
const PREFIX = `https://example.com/floorp-qa-g1-limits-size/${RUN}`;
const BACKUP_DIR = PathUtils.join(PathUtils.profileDir, "workspaces", "backup");
const SYSTEM = Services.scriptSecurityManager.getSystemPrincipal();

function browserWindows(): BackupTestWindow[] {
  const windows: BackupTestWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as BackupTestWindow;
    if (!win.closed && win.gBrowser) windows.push(win);
  }
  return windows;
}

function stateTab(
  win: BackupTestWindow,
  state: Record<string, unknown>,
): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: SYSTEM,
    inBackground: true,
  }) as XULElement;
  SessionStore.setTabState(tab, JSON.stringify(state));
  return tab;
}

function urlTab(win: BackupTestWindow, url: string, title = url): XULElement {
  return stateTab(win, { entries: [{ url, title }], index: 1 });
}

function lazyTabs(
  win: BackupTestWindow,
  count: number,
  label: string,
): XULElement[] {
  const tabs: XULElement[] = [];
  for (let index = 0; index < count; index++) {
    tabs.push(
      win.gBrowser.addTab(`${PREFIX}/${label}-${index}`, {
        triggeringPrincipal: SYSTEM,
        inBackground: true,
        createLazyBrowser: true,
        skipAnimation: true,
      } as Record<string, unknown>) as XULElement,
    );
  }
  return tabs;
}

function tabHolds(tab: XULElement, needle: string): boolean {
  try {
    return SessionStore.getTabState(tab).includes(needle);
  } catch {
    return false;
  }
}

async function readBackup(fileName: string): Promise<WorkspaceBackupFile> {
  const bytes = await IOUtils.read(PathUtils.join(BACKUP_DIR, fileName), {
    decompress: true,
  });
  return JSON.parse(new TextDecoder().decode(bytes)) as WorkspaceBackupFile;
}

function entryUrls(tab: WorkspaceBackupTab): string[] {
  const entries = tab.state?.entries;
  return Array.isArray(entries)
    ? entries.map((entry) => String((entry as { url?: unknown }).url ?? ""))
    : [];
}

function allTabs(file: WorkspaceBackupFile): WorkspaceBackupTab[] {
  return file.windows.flatMap((window) => window.tabs);
}

function tabsMatching(
  file: WorkspaceBackupFile,
  needle: string,
): WorkspaceBackupTab[] {
  return allTabs(file).filter((tab) =>
    (tab.url ?? "").includes(needle) ||
    entryUrls(tab).some((url) => url.includes(needle))
  );
}

function oneTab(file: WorkspaceBackupFile, needle: string): WorkspaceBackupTab {
  const tabs = tabsMatching(file, needle);
  assertEquals(tabs.length, 1, `exactly one captured tab holds ${needle}`);
  return tabs[0];
}

function warnedSince(since: number, text: string): boolean {
  const storage =
    (Cc as Record<string, { getService(iface: unknown): unknown }>)[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage) as {
      getEvents(): ConsoleEvent[];
    };
  return storage.getEvents().some((event) =>
    event.level === "warn" && (event.timeStamp ?? 0) >= since &&
    (event.arguments ?? []).some((arg) => String(arg).includes(text))
  );
}

function readStore(): StoreShape {
  return JSON.parse(
    Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
  ) as StoreShape;
}

function writeStore(store: StoreShape): void {
  Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, JSON.stringify(store));
}

async function listDir(): Promise<Set<string>> {
  return new Set(
    (await IOUtils.getChildren(BACKUP_DIR).catch(() => [] as string[])).map((
      path,
    ) => PathUtils.filename(path)),
  );
}

async function newFiles(before: Set<string>): Promise<string[]> {
  return [...await listDir()].filter((name) => !before.has(name));
}

async function rejection(promise: Promise<unknown>): Promise<string> {
  try {
    await promise;
  } catch (error) {
    // Errors thrown in another window's global fail `instanceof Error`.
    const message = (error as { message?: unknown } | null)?.message;
    return typeof message === "string" ? message : String(error);
  }
  return "(resolved)";
}

async function withBackupCleanup(check: () => Promise<void>): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const tab of win.gBrowser.tabs) {
      attributions.set(
        tab as XULElement,
        (tab as XULElement).getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      );
    }
  }
  const before = await listDir();
  try {
    await check();
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const name of await newFiles(before)) {
      await IOUtils.remove(PathUtils.join(BACKUP_DIR, name), {
        ignoreAbsent: true,
      });
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

function withTestWindow(
  check: (
    win: BackupTestWindow,
    backup: WorkspacesBackupFuncs,
  ) => Promise<void>,
  workspaceEnabled = true,
): Promise<void> {
  return withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API is available");
      await check(win, backup);
    }, workspaceEnabled)
  );
}

function newWorkspaceId(): string {
  return Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
}

// Keeps the shared helpers referenced in every split file.
void [
  settle,
  waitFor,
  urlTab,
  lazyTabs,
  tabHolds,
  oneTab,
  warnedSince,
  readStore,
  writeStore,
  rejection,
  withTestWindow,
  newWorkspaceId,
  WORKSPACE_BACKUP_LIMITS,
];

// C-12
async function testDecompressedSizeLimit(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    urlTab(win, `${PREFIX}/c12`);
    await settle();
    const before = await listDir();
    const encoderClass = (win as unknown as { TextEncoder: typeof TextEncoder })
      .TextEncoder;
    const proto = encoderClass.prototype;
    const original = proto.encode;
    proto.encode = function (this: TextEncoder, input?: string): Uint8Array {
      if (typeof input === "string" && input.startsWith('{"version":2')) {
        return {
          length: WORKSPACE_BACKUP_LIMITS.maxDecompressedBytes + 1,
        } as unknown as Uint8Array;
      }
      return original.call(this, input);
    } as typeof proto.encode;
    let message: string;
    try {
      message = await rejection(backup.backupNow());
    } finally {
      proto.encode = original;
    }
    assertEquals(message, "backup-too-large", "C-12 backup-too-large");
    assertEquals(
      (await newFiles(before)).join(","),
      "",
      "C-12 no file (or temp file) is left",
    );
  });
}

// C-13
async function testCompressedSizeLimit(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    urlTab(win, `${PREFIX}/c13`);
    await settle();
    const before = await listDir();
    const io = (win as unknown as { IOUtils: typeof IOUtils }).IOUtils;
    const original = io.stat;
    const pattern = /backup-\d{8}-\d{6}-[0-9a-f]{8}\.jsonlz4$/i;
    const patched = (path: string): Promise<FileInfo> =>
      original.call(io, path).then((info: FileInfo) =>
        pattern.test(path) && !before.has(PathUtils.filename(path))
          ? { ...info, size: WORKSPACE_BACKUP_LIMITS.maxFileBytes + 1 }
          : info
      );
    Object.defineProperty(io, "stat", {
      configurable: true,
      writable: true,
      value: patched,
    });
    assert(io.stat === patched, "IOUtils.stat can be stubbed");
    let message: string;
    try {
      message = await rejection(backup.backupNow());
    } finally {
      Object.defineProperty(io, "stat", {
        configurable: true,
        writable: true,
        value: original,
      });
    }
    assertEquals(message, "backup-too-large", "C-13 backup-too-large");
    assertEquals(
      (await newFiles(before)).join(","),
      "",
      "C-13 the written file is deleted",
    );
  });
}

// C-14
async function testMultibyteStateSize(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    for (let index = 0; index < 50; index++) {
      urlTab(
        win,
        `${PREFIX}/c14-small-${index}`,
        `日本語タイトル🐱${index}`.repeat(20),
      );
    }
    // 400k code units fit a char budget but are 1.2 MB as UTF-8.
    urlTab(win, `${PREFIX}/c14-over`, "あ".repeat(400_000));
    // 300k code units are 0.9 MB as UTF-8 and stay within the limit.
    urlTab(win, `${PREFIX}/c14-under`, "あ".repeat(300_000));
    await settle();
    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    assertEquals(oneTab(file, "c14-over").state, null, "C-14 bytes, not chars");
    assert(oneTab(file, "c14-under").state !== null, "C-14 0.9 MB state kept");
    assertEquals(
      tabsMatching(file, "c14-small-").length,
      50,
      "C-14 multibyte titles saved",
    );
    assert(
      (await backup.listBackups()).some((entry) =>
        entry.fileName === summary.fileName
      ),
      "C-14 file passes the reader validation",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1LimitsSize.test.ts", [
    { name: "C-12 decompressed size limit", fn: testDecompressedSizeLimit },
    { name: "C-13 compressed size limit", fn: testCompressedSizeLimit },
    { name: "C-14 multibyte state size", fn: testMultibyteStateSize },
  ]);
}
