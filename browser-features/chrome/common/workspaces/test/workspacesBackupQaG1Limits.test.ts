// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup limit and truncation cases C-05 .. C-09.

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
const PREFIX = `https://example.com/floorp-qa-g1-limits/${RUN}`;
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

// C-05
async function testWorkspaceCountLimit(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const store = readStore();
    const template = store.data[0]?.[1];
    assert(template, "store has a workspace");
    const added: string[] = [];
    for (
      let index = 0;
      index < WORKSPACE_BACKUP_LIMITS.maxWorkspaces + 1;
      index++
    ) {
      const id = newWorkspaceId();
      added.push(id);
      store.data.push([id, {
        ...template,
        name: `qa-c05-${index}`,
        isDefault: false,
        isSelected: false,
      }]);
      store.order.push(id);
    }
    writeStore(store);
    await settle();
    const keptTab = urlTab(win, `${PREFIX}/c05-kept`);
    const droppedTab = urlTab(win, `${PREFIX}/c05-dropped`);
    await settle();
    keptTab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, added[0]);
    droppedTab.setAttribute(
      WORKSPACE_TAB_ATTRIBUTION_ID,
      added[added.length - 1],
    );
    const since = Date.now();

    const file = await readBackup((await backup.backupNow()).fileName);
    const state = file.workspaceState;
    assert(state, "workspace state exists");
    assertEquals(state.workspaces.length, 200, "C-05 200 workspaces are kept");
    assertEquals(state.order.length, 200, "C-05 order has 200 entries");
    assert(
      !state.order.includes(added[added.length - 1] as never),
      "C-05 the 201st+ workspace is dropped",
    );
    assertEquals(
      oneTab(file, "c05-dropped").workspaceId,
      undefined,
      "C-05 attribution to a dropped workspace is removed",
    );
    assertEquals(
      oneTab(file, "c05-kept").workspaceId as string | undefined,
      added[0],
      "C-05 attribution to a kept workspace stays",
    );
    assert(
      warnedSince(since, "workspace count exceeds backup limit"),
      "C-05 warning is logged",
    );
  });
}

// C-06 / C-07 / C-08
async function testStringClamps(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const { workspacesFuncs } = win;
    assert(workspacesFuncs, "workspace API");
    const original = workspacesFuncs.getSelectedWorkspaceID();
    const id = workspacesFuncs.createNoNameWorkspace();
    await settle();
    workspacesFuncs.changeWorkspace(original);
    await settle();
    const longName = "名".repeat(300);
    const longIcon = `data:image/svg+xml,${"x".repeat(3000)}`;
    const store = readStore();
    store.data = store.data.map(([key, value]) =>
      key === id ? [key, { ...value, name: longName, icon: longIcon }] : [
        key,
        value,
      ]
    );
    writeStore(store);
    const title = "T".repeat(5000);
    const titled = urlTab(win, `${PREFIX}/c06`, title);
    await settle();
    // tabbrowser already cuts session titles to 256 characters, so the label
    // is set directly to reach the backup's own 4096 clamp.
    titled.setAttribute("label", title);

    const file = await readBackup((await backup.backupNow()).fileName);
    const tab = oneTab(file, "c06");
    assertEquals(tab.title?.length, 4096, "C-06 title clamped to 4096");
    assert(title.startsWith(tab.title ?? "-"), "C-06 title is a prefix");
    const ws = file.workspaceState?.workspaces.find((entry) =>
      entry.workspaceId === id
    );
    assert(ws, "workspace is captured");
    assertEquals(ws.name.length, 256, "C-07 name clamped to 256");
    assertEquals(ws.name, longName.slice(0, 256), "C-07 name prefix");
    assertEquals(ws.icon, null, "C-08 oversized icon becomes null");
  });
}

// C-09
async function testOversizedState(): Promise<void> {
  await withTestWindow(async (win, backup) => {
    const pad = "x".repeat(60_000);
    const entries = Array.from({ length: 20 }, (_, index) => ({
      url: `${PREFIX}/c09-${index}?${pad}`,
      title: `c09-${index}`,
    }));
    stateTab(win, { entries, index: 20 });
    await settle();
    const since = Date.now();
    const file = await readBackup((await backup.backupNow()).fileName);
    const tab = oneTab(file, "c09-19");
    assertEquals(tab.state, null, "C-09 state is dropped");
    assertEquals(tab.url, entries[19].url, "C-09 URL is kept");
    assert(
      warnedSince(since, "dropping oversized tab state"),
      "C-09 warning is logged",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1Limits.test.ts", [
    { name: "C-05 workspace count limit", fn: testWorkspaceCountLimit },
    { name: "C-06/C-07/C-08 string clamps", fn: testStringClamps },
    { name: "C-09 oversized state", fn: testOversizedState },
  ]);
}
