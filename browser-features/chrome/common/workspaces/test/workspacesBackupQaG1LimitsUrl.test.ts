// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup limit and truncation cases C-10 / C-11.

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
const PREFIX = `https://example.com/floorp-qa-g1-limits-url/${RUN}`;
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

// C-10
async function testLongUrlWithState(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const longUrl = `${PREFIX}/c10?${"u".repeat(70_000)}`;
    const original = urlTab(win, longUrl, "c10");
    await settle();
    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    const tab = oneTab(file, "c10?");
    assertEquals(tab.url?.length, 65536, "C-10 tab.url is clamped (concern)");
    assertEquals(entryUrls(tab)[0], longUrl, "C-10 state keeps the full URL");

    const result = await backup.restoreBackup(summary.fileName);
    assert(result.success, `C-10 restore succeeds: ${JSON.stringify(result)}`);
    await settle();
    assert(!original.isConnected, "C-10 original tab was replaced");
    assert(
      [...win.gBrowser.tabs].some((candidate) =>
        tabHolds(candidate as XULElement, longUrl)
      ),
      "C-10 the full URL survives the restore",
    );
  });
}

// C-11
async function testLongUrlWithoutState(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const longUrl = `${PREFIX}/c11?${"v".repeat(70_000)}`;
    const entries = Array.from({ length: 15 }, (_, index) => ({
      url: index === 14
        ? longUrl
        : `${PREFIX}/c11-${index}?${"w".repeat(70_000)}`,
      title: `c11-${index}`,
    }));
    const original = stateTab(win, { entries, index: 15 });
    await settle();
    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    const captured = file.windows.flatMap((window) => window.tabs).filter((
      candidate,
    ) => candidate.url?.includes("c11?"));
    assertEquals(
      captured.length,
      0,
      "C-11 a tab whose URL would be clamped without a state is not captured",
    );

    const result = await backup.restoreBackup(summary.fileName);
    assert(result.success, `C-11 restore succeeds: ${JSON.stringify(result)}`);
    await settle();
    const survivors = [...win.gBrowser.tabs].filter((candidate) =>
      tabHolds(candidate as XULElement, longUrl)
    );
    assert(
      original.isConnected || survivors.length > 0,
      `C-11 the original URL is not lost (original connected: ${original.isConnected}, tabs with full URL: ${survivors.length})`,
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1LimitsUrl.test.ts", [
    { name: "C-10 long URL with state", fn: testLongUrlWithState },
    { name: "C-11 long URL without state", fn: testLongUrlWithoutState },
  ]);
}
