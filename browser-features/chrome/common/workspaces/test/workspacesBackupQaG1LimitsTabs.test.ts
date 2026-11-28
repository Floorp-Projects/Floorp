// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup limit and truncation cases C-01 / C-04.

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
const PREFIX = `https://example.com/floorp-qa-g1-limits-tabs/${RUN}`;
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

// C-01
async function testPerWindowTabLimit(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const tabs = lazyTabs(win, 1000, "c01");
    win.gBrowser.pinTab(tabs[10]);
    win.gBrowser.pinTab(tabs[11]);
    win.gBrowser.selectedTab = tabs[999];
    await settle();
    assertEquals(win.gBrowser.tabs.length, 1001, "C-01 1001 tabs");
    const since = Date.now();
    const file = await readBackup((await backup.backupNow()).fileName);
    const window = file.windows.find((candidate) =>
      candidate.tabs.some((tab) => (tab.url ?? "").includes("/c01-"))
    );
    assert(window, "C-01 test window captured");
    assertEquals(window.tabs.length, 1000, "C-01 1000 tabs saved");
    const selected = window.tabs.filter((tab) => tab.isSelected);
    assertEquals(selected.length, 1, "C-01 selected tab kept");
    assert(
      (selected[0].url ?? "").endsWith("/c01-999"),
      "C-01 the selected (last) tab is kept",
    );
    for (const index of [10, 11]) {
      const pinned = window.tabs.find((tab) =>
        (tab.url ?? "").endsWith(`/c01-${index}`)
      );
      assert(pinned?.pinned, `C-01 pinned tab ${index} kept`);
    }
    assert(
      !window.tabs.some((tab) => (tab.url ?? "").endsWith("/c01-998")),
      "C-01 the last unpinned, unselected tab is the one dropped",
    );
    assert(warnedSince(since, "backup truncated to limits"), "C-01 warning");
  }, false);
}

// C-04
async function testDroppedTabsSurviveRestore(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    urlTab(win, `${PREFIX}/c04-base`);
    await settle();
    const base = await backup.backupNow();
    const tabs = lazyTabs(win, 999, "c04");
    await settle();
    assertEquals(win.gBrowser.tabs.length, 1001, "C-04 1001 tabs");
    const last = tabs[998];
    const middle = tabs[500];
    const result = await backup.restoreBackup(base.fileName);
    assert(result.success, `C-04 restore succeeds: ${JSON.stringify(result)}`);
    await settle();
    assert(!middle.isConnected, "C-04 captured tabs are replaced");
    assert(last.isConnected, "C-04 tab dropped by the limit stays open");
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1LimitsTabs.test.ts", [
    { name: "C-01 per-window tab limit", fn: testPerWindowTabLimit },
    {
      name: "C-04 dropped tabs survive restore",
      fn: testDroppedTabsSurviveRestore,
    },
  ]);
}
