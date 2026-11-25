// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g3: workspace-mode replace restore (F-*) and pre-restore safety
// copies (J-*).
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { settle, withWindows } from "./workspace-last-tab-test-utils.ts";
import {
  WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "../utils/workspaces-static-names.ts";
import {
  buildBackupFile,
  formatBackupFileName,
  isWorkspaceBackupFile,
  serializeBackupConfig,
} from "../utils/workspaces-backup-helpers.ts";
import {
  DEFAULT_WORKSPACE_BACKUP_CONFIG,
  type WorkspaceBackupFile,
  type WorkspaceBackupKind,
  type WorkspaceBackupSummary,
  type WorkspaceBackupTab,
  type WorkspaceBackupWorkspace,
} from "../utils/backup-types.ts";
import type { TWorkspaceID } from "../utils/type.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type QaWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
}

interface StoredWorkspace {
  name: string;
  icon?: string | null;
  userContextId: number;
}

interface StoredWorkspaces {
  defaultID: string;
  order: string[];
  data: Record<string, StoredWorkspace>;
}

interface CraftedTab {
  url: string;
  workspaceId?: string;
  selected?: boolean;
  lastShown?: string;
  userContextId?: number;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as { E10SUtils: { serializePrincipal(principal: unknown): string } };

const PREFIX = `https://example.com/floorp-qa-g3/${Date.now()}`;

const backupDirectory = (): string =>
  PathUtils.join(PathUtils.profileDir, "workspaces", "backup");

const newWorkspaceId = (): string =>
  Services.uuid.generateUUID().toString().replace(/[{}]/g, "");

function browserWindows(): QaWindow[] {
  const windows: QaWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as QaWindow;
    if (!win.closed) windows.push(win);
  }
  return windows;
}

function tabUrls(tab: XULElement): string[] {
  const state = JSON.parse(SessionStore.getTabState(tab)) as {
    entries?: { url?: string }[];
  };
  return (state.entries ?? []).map((entry) => entry.url ?? "");
}

function tabsWith(win: QaWindow, url: string): XULElement[] {
  return ([...win.gBrowser.tabs] as XULElement[]).filter((tab) =>
    tabUrls(tab).includes(url)
  );
}

function oneTab(win: QaWindow, url: string): XULElement {
  const tabs = tabsWith(win, url);
  assertEquals(tabs.length, 1, `exactly one tab holds ${url}`);
  return tabs[0];
}

function tabState(url: string, userContextId = 0): Record<string, unknown> {
  return {
    entries: [{
      url,
      triggeringPrincipal_base64: E10SUtils.serializePrincipal(
        Services.scriptSecurityManager.createContentPrincipal(
          Services.io.newURI(url),
          { userContextId },
        ),
      ),
    }],
    index: 1,
    userContextId,
  };
}

function openTab(win: QaWindow, url: string): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
  }) as XULElement;
  SessionStore.setTabState(tab, JSON.stringify(tabState(url)));
  return tab;
}

function readStore(): StoredWorkspaces {
  const raw = JSON.parse(
    Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME, "{}"),
  ) as {
    defaultID: string;
    order: string[];
    data: [string, StoredWorkspace][];
  };
  return {
    defaultID: raw.defaultID,
    order: raw.order,
    data: Object.fromEntries(raw.data),
  };
}

async function readBackupUrls(fileName: string): Promise<string[]> {
  const bytes = await IOUtils.read(
    PathUtils.join(backupDirectory(), fileName),
    { decompress: true },
  );
  const file = JSON.parse(
    new TextDecoder().decode(bytes),
  ) as WorkspaceBackupFile;
  return file.windows.flatMap((window) =>
    window.tabs.map((tab) => tab.url ?? "")
  );
}

function workspaceIdByName(name: string): string {
  const store = readStore();
  const ids = store.order.filter((id) => store.data[id]?.name === name);
  assertEquals(ids.length, 1, `exactly one workspace is named ${name}`);
  return ids[0];
}

function apis(win: QaWindow): {
  backup: WorkspacesBackupFuncs;
  workspaces: NonNullable<QaWindow["workspacesFuncs"]>;
} {
  const backup = win.workspacesBackupFuncs;
  const workspaces = win.workspacesFuncs;
  assert(backup && workspaces, "backup and workspace APIs are available");
  return { backup, workspaces };
}

function craftFile(
  kind: WorkspaceBackupKind,
  workspaces: WorkspaceBackupWorkspace[],
  tabs: CraftedTab[],
  current: string | null,
  capturedAt = Date.now(),
): WorkspaceBackupFile {
  const backupTabs: WorkspaceBackupTab[] = tabs.map((tab) => {
    const entry: WorkspaceBackupTab = {
      state: tabState(tab.url, tab.userContextId ?? 0),
      title: null,
      url: tab.url,
      pinned: false,
      isSelected: tab.selected === true,
      userContextId: tab.userContextId ?? 0,
      lastShownWorkspaceId: (tab.lastShown ?? null) as TWorkspaceID | null,
    };
    if (tab.workspaceId) entry.workspaceId = tab.workspaceId as TWorkspaceID;
    return entry;
  });
  return buildBackupFile(
    kind,
    workspaces.length > 0 ? "workspaces" : "tabs",
    capturedAt,
    [{ isFocused: true, tabs: backupTabs }],
    workspaces.length > 0
      ? {
        workspaces,
        order: workspaces.map((ws) => ws.workspaceId),
        defaultID: workspaces[0].workspaceId,
        currentWorkspaceId: current as TWorkspaceID | null,
      }
      : undefined,
  );
}

async function writeFile(file: WorkspaceBackupFile): Promise<string> {
  assert(isWorkspaceBackupFile(file), "crafted backup file is valid");
  const shortId = Services.uuid.generateUUID().toString().replace(
    /[{}-]/g,
    "",
  ).slice(0, 8);
  const fileName = formatBackupFileName(file.capturedAt, shortId);
  await IOUtils.write(
    PathUtils.join(backupDirectory(), fileName),
    new TextEncoder().encode(JSON.stringify(file)),
    { compress: true },
  );
  return fileName;
}

async function listKind(
  backup: WorkspacesBackupFuncs,
  kind: WorkspaceBackupKind,
): Promise<WorkspaceBackupSummary[]> {
  return (await backup.listBackups()).filter((item) => item.kind === kind);
}

function restorePref(name: string, had: boolean, value: string): void {
  if (had) Services.prefs.setStringPref(name, value);
  else Services.prefs.clearUserPref(name);
}

/**
 * Moves the profile's existing backups aside, and on exit deletes every
 * backup the test produced, restores the originals, the backup prefs, other
 * windows, and the workspace attribution of the harness tabs.
 */
async function isolated(check: () => Promise<void>): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  const disabledHistory = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const tab of win.gBrowser.tabs as XULElement[]) {
      attributions.set(tab, tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID));
      disabledHistory.set(tab, tab.getAttribute("floorp-disablehistory"));
    }
  }
  const hadLastRun = Services.prefs.prefHasUserValue(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
  );
  const lastRun = Services.prefs.getStringPref(
    WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
    "",
  );
  const hadConfig = Services.prefs.prefHasUserValue(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
  );
  const config = Services.prefs.getStringPref(
    WORKSPACE_BACKUP_CONFIG_PREF_NAME,
    "",
  );
  const directory = backupDirectory();
  const stash = PathUtils.join(
    PathUtils.profileDir,
    "workspaces",
    `backup-qa-g3-stash-${Date.now()}`,
  );
  await IOUtils.makeDirectory(directory, { ignoreExisting: true });
  await IOUtils.setPermissions(directory, 0o755);
  await IOUtils.makeDirectory(stash, { ignoreExisting: true });
  const stashed: string[] = [];
  for (const path of await IOUtils.getChildren(directory)) {
    const name = PathUtils.filename(path);
    await IOUtils.move(path, PathUtils.join(stash, name));
    stashed.push(name);
  }
  try {
    await check();
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    await settle();
    await IOUtils.setPermissions(directory, 0o755);
    for (const path of await IOUtils.getChildren(directory)) {
      await IOUtils.remove(path, { recursive: true });
    }
    for (const name of stashed) {
      await IOUtils.move(
        PathUtils.join(stash, name),
        PathUtils.join(directory, name),
      );
    }
    await IOUtils.remove(stash, { recursive: true });
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
    for (const [tab, value] of disabledHistory) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute("floorp-disablehistory");
      else tab.setAttribute("floorp-disablehistory", value);
    }
    restorePref(WORKSPACE_BACKUP_CONFIG_PREF_NAME, hadConfig, config);
    restorePref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME, hadLastRun, lastRun);
  }
}

const inWindow = (
  check: (win: QaWindow, open: () => Promise<QaWindow>) => Promise<void>,
  closeWithLastTab = false,
  exitWorkspace = false,
  workspaceEnabled = true,
): Promise<void> =>
  isolated(() =>
    withWindows(closeWithLastTab, exitWorkspace, async (open) => {
      const win = (await open()) as QaWindow;
      await check(win, async () => (await open()) as QaWindow);
    }, workspaceEnabled)
  );

function threeWorkspaceFile(tag: string): {
  file: WorkspaceBackupFile;
  ids: string[];
  names: string[];
} {
  const ids = [newWorkspaceId(), newWorkspaceId(), newWorkspaceId()];
  const names = [`${tag}-A`, `${tag}-B`, `${tag}-C`];
  const file = craftFile(
    "manual",
    [
      {
        workspaceId: ids[0] as TWorkspaceID,
        name: names[0],
        userContextId: 0,
        icon: "briefcase",
      },
      {
        workspaceId: ids[1] as TWorkspaceID,
        name: names[1],
        userContextId: 0,
        icon: "cart",
      },
      {
        workspaceId: ids[2] as TWorkspaceID,
        name: names[2],
        userContextId: 0,
        icon: null,
      },
    ],
    [
      { url: `${PREFIX}/${tag}/a1`, workspaceId: ids[0] },
      { url: `${PREFIX}/${tag}/a2`, workspaceId: ids[0], selected: true },
      { url: `${PREFIX}/${tag}/b1`, workspaceId: ids[1] },
      { url: `${PREFIX}/${tag}/b2`, workspaceId: ids[1], lastShown: ids[1] },
      { url: `${PREFIX}/${tag}/c1`, workspaceId: ids[2] },
    ],
    ids[1],
  );
  return { file, ids, names };
}

// F-04 / F-05 / F-06 / F-07 / F-08 / F-10 / F-11
async function testRestoreMapsWorkspaces(): Promise<void> {
  await inWindow(async (win) => {
    const { backup, workspaces } = apis(win);
    const { file, ids, names } = threeWorkspaceFile("map");
    const fileName = await writeFile(file);
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    await settle();

    const store = readStore();
    const restored = names.map(workspaceIdByName);
    for (const id of restored) {
      assert(!ids.includes(id), "F-04: workspace IDs are freshly assigned");
      assert(workspaces.isWorkspaceID(id), "F-04: new ID is a live workspace");
    }
    assertEquals(store.order.length, 3, "F-05: only restored WS remain");
    assertEquals(
      JSON.stringify(store.order),
      JSON.stringify(restored),
      "F-05: workspace order follows the backup",
    );
    assertEquals(store.defaultID, restored[0], "F-06: default WS restored");
    assertEquals(
      workspaces.getSelectedWorkspaceID(),
      restored[1],
      "F-06: shown WS restored",
    );
    assertEquals(store.data[restored[0]].icon, "briefcase", "F-07: icon A");
    assertEquals(store.data[restored[1]].icon, "cart", "F-07: icon B");
    for (const [index, suffix] of ["a1", "a2", "b1", "b2", "c1"].entries()) {
      const tab = oneTab(win, `${PREFIX}/map/${suffix}`);
      const expected = restored[Math.floor(index / 2)];
      assertEquals(
        tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
        expected,
        `F-08: ${suffix} keeps its workspace`,
      );
      assertEquals(
        tab.hidden,
        expected !== restored[1],
        `F-11: ${suffix} is visible only in the shown workspace`,
      );
    }
    assertEquals(
      oneTab(win, `${PREFIX}/map/b2`).getAttribute(WORKSPACE_LAST_SHOW_ID),
      restored[1],
      "F-10: lastShownWorkspaceId is remapped",
    );
    workspaces.changeWorkspace(restored[2]);
    await settle();
    workspaces.changeWorkspace(restored[1]);
    await settle();
    assertEquals(
      win.gBrowser.selectedTab,
      oneTab(win, `${PREFIX}/map/b2`),
      "F-10: switching back selects the last-shown tab",
    );
  });
}

// F-07: workspace container (userContextId) is restored
async function testRestoreKeepsContainer(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const id = newWorkspaceId();
    const fileName = await writeFile(craftFile("manual", [
      {
        workspaceId: id as TWorkspaceID,
        name: "ctx-A",
        userContextId: 1,
        icon: "fruit",
      },
    ], [{
      url: `${PREFIX}/ctx/a`,
      workspaceId: id,
      userContextId: 1,
      selected: true,
    }], id));
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    const restored = workspaceIdByName("ctx-A");
    assertEquals(readStore().data[restored].userContextId, 1, "WS container");
    assertEquals(readStore().data[restored].icon, "fruit", "WS icon");
    assertEquals(
      oneTab(win, `${PREFIX}/ctx/a`).getAttribute("usercontextid"),
      "1",
      "tab container",
    );
  });
}

// F-09: tabs whose workspace is missing from the backup join the first one
async function testUnknownWorkspaceTab(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const ids = [newWorkspaceId(), newWorkspaceId()];
    const fileName = await writeFile(craftFile("manual", [
      { workspaceId: ids[0] as TWorkspaceID, name: "unk-A", userContextId: 0 },
      { workspaceId: ids[1] as TWorkspaceID, name: "unk-B", userContextId: 0 },
    ], [
      { url: `${PREFIX}/unk/b`, workspaceId: ids[1], selected: true },
      { url: `${PREFIX}/unk/orphan`, workspaceId: newWorkspaceId() },
      { url: `${PREFIX}/unk/none` },
    ], ids[1]));
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    const first = workspaceIdByName("unk-A");
    for (const suffix of ["orphan", "none"]) {
      assertEquals(
        oneTab(win, `${PREFIX}/unk/${suffix}`).getAttribute(
          WORKSPACE_TAB_ATTRIBUTION_ID,
        ),
        first,
        `${suffix} tab joins the first restored workspace`,
      );
    }
  });
}

// F-12 / F-13 / F-14 / J-01
// Code intent (workspacesService.restoreWorkspaceState + collectTabsNotIn):
// tabs captured by the safety copy are replaced (recoverable from it); only
// tabs the safety copy could not hold are kept and moved into the shown WS.
async function testTabsOpenedAfterBackup(): Promise<void> {
  await inWindow(async (win) => {
    const { backup, workspaces } = apis(win);
    const kept = openTab(win, `${PREFIX}/after/kept`);
    const swapped = openTab(win, `${PREFIX}/after/swapped`);
    await settle();
    const saved = await backup.backupNow();
    win.gBrowser.removeTab(swapped, { animate: false });
    openTab(win, `${PREFIX}/after/later`);
    const uncapturable = win.gBrowser.addTab("about:robots", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: true,
    }) as XULElement;
    await settle();
    const before = (await listKind(backup, "pre-restore")).length;
    const result = await backup.restoreBackup(saved.fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    await settle();
    const preRestore = await listKind(backup, "pre-restore");
    assertEquals(preRestore.length, before + 1, "J-01: one safety copy made");
    const safetyUrls = await readBackupUrls(preRestore[0].fileName);
    assert(
      safetyUrls.includes(`${PREFIX}/after/later`),
      "F-13: the tab opened after the backup is held by the safety copy",
    );
    assert(
      !safetyUrls.includes(`${PREFIX}/after/swapped`),
      "J-01: the safety copy reflects the state right before the restore",
    );
    assertEquals(
      tabsWith(win, `${PREFIX}/after/later`).length,
      0,
      "F-13/F-14: a captured later tab is replaced (code intent)",
    );
    assert(uncapturable.isConnected, "F-12: an uncaptured tab stays open");
    assertEquals(
      uncapturable.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
      workspaces.getSelectedWorkspaceID(),
      "F-12: the kept tab joins the shown restored workspace",
    );
    assertEquals(uncapturable.hidden, false, "F-12: the kept tab is visible");
    assert(!kept.isConnected, "captured tab is replaced");
    assertEquals(tabsWith(win, `${PREFIX}/after/kept`).length, 1, "kept x1");
    assertEquals(
      tabsWith(win, `${PREFIX}/after/swapped`).length,
      1,
      "F-14: closed tab comes back once",
    );
  });
}

// F-15 / F-16
async function testOtherWindowTabs(): Promise<void> {
  await inWindow(async (win, open) => {
    const { backup } = apis(win);
    const other = await open();
    const otherTab = openTab(other, `${PREFIX}/other/b`);
    await settle();
    const saved = await backup.backupNow();
    const otherLater = openTab(other, `${PREFIX}/other/later`);
    const otherTabs = [...other.gBrowser.tabs] as XULElement[];
    await settle();
    const result = await backup.restoreBackup(saved.fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    await settle();
    assert(!other.closed, "F-15: other window stays open");
    for (const tab of otherTabs) {
      assert(tab.isConnected, "F-15: other window tabs are not closed");
    }
    const live = new Set(readStore().order);
    for (const tab of [otherTab, otherLater]) {
      const id = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      assert(
        id !== null && live.has(id),
        `F-16: other window tab points at a restored workspace (${id})`,
      );
    }
    assertEquals(
      tabsWith(win, `${PREFIX}/other/b`).length,
      1,
      "known design: the other window's tab is also restored here",
    );
  });
}

function expectedTabCount(
  win: QaWindow,
  summary: WorkspaceBackupSummary,
): number {
  const preserved = ([...win.gBrowser.tabs] as XULElement[]).filter((tab) => {
    const urls = tabUrls(tab);
    return urls.length === 0 ||
      urls.every((url) => !/^(https?|file):/.test(url));
  }).length;
  return summary.tabCount + preserved;
}

// F-17 / F-18 / F-19
function lastTabCase(closeWithLastTab: boolean, exitWorkspace: boolean) {
  return async (): Promise<void> => {
    await inWindow(
      async (win) => {
        const { backup } = apis(win);
        const only = win.gBrowser.tabs[0] as XULElement;
        SessionStore.setTabState(
          only,
          JSON.stringify(tabState(`${PREFIX}/last/only`)),
        );
        await settle();
        const saved = await backup.backupNow();
        const expected = expectedTabCount(win, saved);
        const result = await backup.restoreBackup(saved.fileName);
        assert(result.success, `restore succeeds (${result.error ?? ""})`);
        await settle();
        assert(!win.closed, "the window survives the restore");
        assertEquals(tabsWith(win, `${PREFIX}/last/only`).length, 1, "tab x1");
        assertEquals(
          win.gBrowser.tabs.length,
          expected,
          "no extra blank tab is left behind",
        );
      },
      closeWithLastTab,
      exitWorkspace,
    );
  };
}

// F-21
async function testRestoreTwice(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const { file, names } = threeWorkspaceFile("twice");
    const fileName = await writeFile(file);
    for (const round of [1, 2]) {
      const result = await backup.restoreBackup(fileName);
      assert(
        result.success,
        `restore ${round} succeeds (${result.error ?? ""})`,
      );
      await settle();
      assertEquals(readStore().order.length, 3, `round ${round}: 3 WS`);
      names.forEach(workspaceIdByName);
      for (const suffix of ["a1", "a2", "b1", "b2", "c1"]) {
        assertEquals(
          tabsWith(win, `${PREFIX}/twice/${suffix}`).length,
          1,
          `round ${round}: ${suffix} is not duplicated`,
        );
      }
    }
  });
}

// F-22
async function testSingleWorkspace(): Promise<void> {
  await inWindow(async (win) => {
    const { backup, workspaces } = apis(win);
    const id = newWorkspaceId();
    const fileName = await writeFile(craftFile(
      "manual",
      [
        { workspaceId: id as TWorkspaceID, name: "single-A", userContextId: 0 },
      ],
      [{ url: `${PREFIX}/single/a`, workspaceId: id, selected: true }],
      id,
    ));
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    const restored = workspaceIdByName("single-A");
    assertEquals(readStore().order.length, 1, "one workspace");
    assertEquals(readStore().defaultID, restored, "it is the default");
    assertEquals(workspaces.getSelectedWorkspaceID(), restored, "it is shown");
  });
}

// F-23
async function testTabsModeIntoWorkspaces(): Promise<void> {
  await inWindow(async (win) => {
    const { backup, workspaces } = apis(win);
    const existing = openTab(win, `${PREFIX}/tabsmode/existing`);
    await settle();
    const storeBefore = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
    const current = workspaces.getSelectedWorkspaceID();
    const fileName = await writeFile(craftFile("manual", [], [
      { url: `${PREFIX}/tabsmode/a`, selected: true },
      { url: `${PREFIX}/tabsmode/b` },
    ], null));
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    await settle();
    assert(existing.isConnected, "existing tabs are kept (additive)");
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      storeBefore,
      "workspace store is untouched",
    );
    for (const suffix of ["a", "b"]) {
      assertEquals(
        oneTab(win, `${PREFIX}/tabsmode/${suffix}`).getAttribute(
          WORKSPACE_TAB_ATTRIBUTION_ID,
        ),
        current,
        `${suffix} joins the current workspace`,
      );
    }
    assertEquals(
      (await listKind(backup, "pre-restore")).length,
      0,
      "additive restore takes no safety copy",
    );
  });
}

// F-24
async function testWorkspacesModeWhileDisabled(): Promise<void> {
  await inWindow(
    async (win) => {
      const backup = win.workspacesBackupFuncs;
      assert(backup, "detached backup API is available");
      Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, true);
      await settle();
      const tabsBefore = [...win.gBrowser.tabs] as XULElement[];
      const id = newWorkspaceId();
      const fileName = await writeFile(craftFile(
        "manual",
        [
          {
            workspaceId: id as TWorkspaceID,
            name: "disabled-A",
            userContextId: 0,
          },
        ],
        [{ url: `${PREFIX}/disabled/a`, workspaceId: id, selected: true }],
        id,
      ));
      const result = await backup.restoreBackup(fileName);
      assert(!result.success, "destructive restore is refused");
      assertEquals(result.error, "workspaces-disabled", "error code");
      assertEquals(tabsWith(win, `${PREFIX}/disabled/a`).length, 0, "no tabs");
      for (const tab of tabsBefore) assert(tab.isConnected, "tabs kept");
      assertEquals(
        (await listKind(backup, "pre-restore")).length,
        0,
        "no safety copy is taken",
      );
    },
    false,
    false,
    false,
  );
}

// J-02
async function testSafetyCopyUndo(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const original = openTab(win, `${PREFIX}/undo/original`);
    await settle();
    const saved = await backup.backupNow();
    win.gBrowser.removeTab(original, { animate: false });
    const edited = openTab(win, `${PREFIX}/undo/edited`);
    await settle();
    const storeBeforeRestore = readStore();
    const namesBefore = storeBeforeRestore.order.map((id) =>
      storeBeforeRestore.data[id].name
    );
    assert(edited.isConnected, "edited tab exists");
    const first = await backup.restoreBackup(saved.fileName);
    assert(first.success, `restore succeeds (${first.error ?? ""})`);
    await settle();
    const [safety] = await listKind(backup, "pre-restore");
    assert(safety, "a safety copy exists");
    assertEquals(
      tabsWith(win, `${PREFIX}/undo/edited`).length,
      0,
      "the edited tab is replaced by the restore",
    );
    const undo = await backup.restoreBackup(safety.fileName);
    assert(undo.success, `undo succeeds (${undo.error ?? ""})`);
    await settle();
    assertEquals(
      tabsWith(win, `${PREFIX}/undo/edited`).length,
      1,
      "the pre-restore tab set is back",
    );
    assertEquals(
      tabsWith(win, `${PREFIX}/undo/original`).length,
      0,
      "tabs that only the restored backup had are gone",
    );
    const storeAfter = readStore();
    assertEquals(
      JSON.stringify(storeAfter.order.map((id) => storeAfter.data[id].name)),
      JSON.stringify(namesBefore),
      "workspace names/order match the state before the first restore",
    );
  });
}

// J-03
async function testSafetyCopyFailureAborts(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    openTab(win, `${PREFIX}/fail/a`);
    await settle();
    const { file } = threeWorkspaceFile("fail");
    const fileName = await writeFile(file);
    const storeBefore = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
    const tabsBefore = [...win.gBrowser.tabs] as XULElement[];
    await IOUtils.setPermissions(backupDirectory(), 0o555);
    let result;
    try {
      result = await backup.restoreBackup(fileName);
    } finally {
      await IOUtils.setPermissions(backupDirectory(), 0o755);
    }
    assert(!result.success, "restore is aborted");
    assertEquals(result.error, "pre-restore-failed", "error code");
    await settle();
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      storeBefore,
      "workspace store is untouched",
    );
    for (const tab of tabsBefore) assert(tab.isConnected, "no tab closed");
    assertEquals(tabsWith(win, `${PREFIX}/fail/a1`).length, 0, "no tab added");
  });
}

// J-04
async function testNothingToBackUp(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const { file, names } = threeWorkspaceFile("empty");
    const fileName = await writeFile(file);
    const tabsBefore: XULElement[] = [];
    for (const w of browserWindows()) {
      for (const tab of w.gBrowser.tabs as XULElement[]) {
        tab.setAttribute("floorp-disablehistory", "true");
        tabsBefore.push(tab);
      }
    }
    const result = await backup.restoreBackup(fileName);
    assert(result.success, `restore proceeds (${result.error ?? ""})`);
    await settle();
    names.forEach(workspaceIdByName);
    for (const tab of tabsBefore) {
      assert(tab.isConnected, "uncaptured tab kept");
    }
    assertEquals(
      (await listKind(backup, "pre-restore")).length,
      0,
      "no empty safety copy is written",
    );
  });
}

async function writeAged(
  kind: WorkspaceBackupKind,
  count: number,
  ageMs: number,
): Promise<string[]> {
  const names: string[] = [];
  for (let i = 0; i < count; i++) {
    const id = newWorkspaceId();
    names.push(
      await writeFile(craftFile(
        kind,
        [
          {
            workspaceId: id as TWorkspaceID,
            name: `aged-${kind}-${i}`,
            userContextId: 0,
          },
        ],
        [{ url: `${PREFIX}/aged/${kind}/${i}`, workspaceId: id }],
        id,
        Date.now() - ageMs - i * 60_000,
      )),
    );
  }
  return names;
}

// J-05 / J-07 / J-08
async function testPreRestoreLimit(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    const agedPre = await writeAged("pre-restore", 12, 20 * 60_000);
    const manual = await writeAged("manual", 12, 60 * 60_000);
    const auto = await writeAged("auto", 3, 2 * 60 * 60_000);
    openTab(win, `${PREFIX}/limit/a`);
    await settle();
    const saved = await backup.backupNow();
    const lastRun = String(Date.now() - 1000);
    Services.prefs.setStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME, lastRun);
    const result = await backup.restoreBackup(saved.fileName);
    assert(result.success, `restore succeeds (${result.error ?? ""})`);
    await settle();
    const pre = (await listKind(backup, "pre-restore")).map((b) => b.fileName);
    assertEquals(pre.length, 10, "J-05: pre-restore copies are capped at 10");
    assert(
      !pre.includes(agedPre[11]) && !pre.includes(agedPre[10]) &&
        !pre.includes(agedPre[9]),
      "J-05: the oldest copies are removed first",
    );
    const manualNow = (await listKind(backup, "manual")).map((b) => b.fileName);
    for (const name of [...manual, saved.fileName]) {
      assert(manualNow.includes(name), `J-07: manual ${name} kept`);
    }
    const autoNow = (await listKind(backup, "auto")).map((b) => b.fileName);
    for (const name of auto) assert(autoNow.includes(name), "J-07: auto kept");
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_BACKUP_LAST_RUN_PREF_NAME),
      lastRun,
      "J-08: the safety copy does not touch lastRun",
    );
  });
}

// J-06
async function testFreshPreRestoreGrace(): Promise<void> {
  await inWindow(async (win) => {
    const { backup } = apis(win);
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_LAST_RUN_PREF_NAME,
      String(Date.now()),
    );
    Services.prefs.setStringPref(
      WORKSPACE_BACKUP_CONFIG_PREF_NAME,
      serializeBackupConfig({
        ...DEFAULT_WORKSPACE_BACKUP_CONFIG,
        maxPreRestoreBackups: 1,
      }),
    );
    await settle();
    const [aged] = await writeAged("pre-restore", 1, 20 * 60_000);
    openTab(win, `${PREFIX}/grace/a`);
    await settle();
    const saved = await backup.backupNow();
    for (const round of [1, 2]) {
      const result = await backup.restoreBackup(saved.fileName);
      assert(
        result.success,
        `restore ${round} succeeds (${result.error ?? ""})`,
      );
      await settle();
    }
    const pre = (await listKind(backup, "pre-restore")).map((b) => b.fileName);
    assert(!pre.includes(aged), "an old copy over the limit is removed");
    assertEquals(pre.length, 2, "both fresh copies are kept despite limit 1");
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG3Restore.test.ts", [
    {
      name: "F-04..F-11 restore remaps workspaces",
      fn: testRestoreMapsWorkspaces,
    },
    { name: "F-07 restore keeps WS container", fn: testRestoreKeepsContainer },
    { name: "F-09 unknown-workspace tab", fn: testUnknownWorkspaceTab },
    {
      name: "F-12/F-13/F-14/J-01 tabs opened after backup",
      fn: testTabsOpenedAfterBackup,
    },
    { name: "F-15/F-16 other window tabs", fn: testOtherWindowTabs },
    { name: "F-17 last tab", fn: lastTabCase(false, false) },
    { name: "F-18 closeWindowWithLastTab", fn: lastTabCase(true, false) },
    { name: "F-19 exit workspace with last tab", fn: lastTabCase(false, true) },
    { name: "F-21 restore twice", fn: testRestoreTwice },
    { name: "F-22 single workspace", fn: testSingleWorkspace },
    { name: "F-23 tabs mode into workspaces", fn: testTabsModeIntoWorkspaces },
    {
      name: "F-24 workspaces mode while disabled",
      fn: testWorkspacesModeWhileDisabled,
    },
    { name: "J-02 safety copy undo", fn: testSafetyCopyUndo },
    {
      name: "J-03 safety copy failure aborts",
      fn: testSafetyCopyFailureAborts,
    },
    { name: "J-04 nothing to back up", fn: testNothingToBackUp },
    { name: "J-05/J-07/J-08 pre-restore limit", fn: testPreRestoreLimit },
    { name: "J-06 fresh pre-restore grace", fn: testFreshPreRestoreGrace },
  ]);
}
