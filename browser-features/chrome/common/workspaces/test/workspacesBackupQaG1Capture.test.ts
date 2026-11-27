// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// QA group g1 (PR #3159): backup capture cases A-03 .. A-32.

import { createRoot } from "solid-js";
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
import {
  type WorkspacesBackupHost,
  WorkspacesBackupService,
} from "../utils/workspaces-backup-service.ts";
import { isWorkspaceBackupFile } from "../utils/workspaces-backup-helpers.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupTab,
  WorkspaceBackupWindow,
} from "../utils/backup-types.ts";
import type { TWorkspace, TWorkspaceID } from "../utils/type.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
  gBrowser: TestWindow["gBrowser"] & {
    pinTab(tab: XULElement): void;
    setIcon(tab: XULElement, url: string): void;
  };
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

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };

const RUN = Date.now();
const PREFIX = `https://example.com/floorp-qa-g1-capture/${RUN}`;
const MARK = `qag1mark${RUN}`;

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
  userContextId = 0,
): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
    userContextId,
  }) as XULElement;
  SessionStore.setTabState(tab, JSON.stringify({ userContextId, ...state }));
  return tab;
}

function urlTab(
  win: BackupTestWindow,
  urls: string[],
  index = urls.length,
  userContextId = 0,
): XULElement {
  return stateTab(
    win,
    { entries: urls.map((url) => ({ url, title: url })), index },
    userContextId,
  );
}

function rawState(tab: XULElement): Record<string, unknown> {
  return JSON.parse(SessionStore.getTabState(tab)) as Record<string, unknown>;
}

function backupPath(fileName: string): string {
  return PathUtils.join(PathUtils.profileDir, "workspaces", "backup", fileName);
}

async function readBackup(fileName: string): Promise<WorkspaceBackupFile> {
  const bytes = await IOUtils.read(backupPath(fileName), { decompress: true });
  return JSON.parse(new TextDecoder().decode(bytes)) as WorkspaceBackupFile;
}

function entryUrls(tab: WorkspaceBackupTab): string[] {
  const entries = tab.state?.entries;
  return Array.isArray(entries)
    ? entries.map((entry) => String((entry as { url?: unknown }).url ?? ""))
    : [];
}

function tabsWithUrl(
  file: WorkspaceBackupFile,
  url: string,
): WorkspaceBackupTab[] {
  return file.windows.flatMap((window) => window.tabs).filter((tab) =>
    tab.url === url || entryUrls(tab).includes(url)
  );
}

function oneTab(file: WorkspaceBackupFile, url: string): WorkspaceBackupTab {
  const tabs = tabsWithUrl(file, url);
  assertEquals(tabs.length, 1, `exactly one captured tab holds ${url}`);
  return tabs[0];
}

function windowWithUrl(
  file: WorkspaceBackupFile,
  url: string,
): WorkspaceBackupWindow {
  const window = file.windows.find((candidate) =>
    candidate.tabs.some((tab) =>
      tab.url === url || entryUrls(tab).includes(url)
    )
  );
  assert(window, `a captured window holds ${url}`);
  return window;
}

function readStore(): StoreShape {
  return JSON.parse(
    Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
  ) as StoreShape;
}

function writeStore(store: StoreShape): void {
  Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, JSON.stringify(store));
}

function api(win: BackupTestWindow): {
  backup: WorkspacesBackupFuncs;
  workspaces: NonNullable<BackupTestWindow["workspacesFuncs"]>;
} {
  const backup = win.workspacesBackupFuncs;
  const workspaces = win.workspacesFuncs;
  assert(backup && workspaces, "backup and workspace APIs are available");
  return { backup, workspaces };
}

/** Deletes backups created by `check` and restores harness attribution. */
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
  const funcs = harnessWindows[0]?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set(
    (await IOUtils.getChildren(
      PathUtils.join(PathUtils.profileDir, "workspaces", "backup"),
    ).catch(() => [] as string[])).map((path) => PathUtils.filename(path)),
  );
  try {
    await check();
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    const after = await IOUtils.getChildren(
      PathUtils.join(PathUtils.profileDir, "workspaces", "backup"),
    ).catch(() => [] as string[]);
    for (const path of after) {
      if (!before.has(PathUtils.filename(path))) {
        await IOUtils.remove(path, { ignoreAbsent: true });
      }
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

function withWorkspaceWindow(
  check: (win: BackupTestWindow) => Promise<void>,
  workspaceEnabled = true,
): Promise<void> {
  return withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      await check(win);
    }, workspaceEnabled)
  );
}

// A-03 / A-05
async function testWorkspaceAttribution(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup, workspaces } = api(win);
    const ids: string[] = [workspaces.getSelectedWorkspaceID()];
    urlTab(win, [`${PREFIX}/a03-ws0-a`]);
    urlTab(win, [`${PREFIX}/a03-ws0-b`]);
    await settle();
    for (const n of [1, 2]) {
      ids.push(workspaces.createNoNameWorkspace());
      await settle();
      urlTab(win, [`${PREFIX}/a03-ws${n}-a`]);
      urlTab(win, [`${PREFIX}/a03-ws${n}-b`]);
      await settle();
    }
    assertEquals(new Set(ids).size, 3, "three distinct workspaces");
    const store = readStore();

    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    assertEquals(file.mode, "workspaces", "A-03 mode=workspaces");
    for (const [n, id] of ids.entries()) {
      for (const suffix of ["a", "b"]) {
        assertEquals(
          oneTab(file, `${PREFIX}/a03-ws${n}-${suffix}`).workspaceId,
          id,
          `A-03 tab ws${n}-${suffix} keeps its workspace`,
        );
      }
    }
    const testWindow = windowWithUrl(file, `${PREFIX}/a03-ws0-a`);
    assert(
      testWindow.tabs.every((tab) => typeof tab.workspaceId === "string"),
      "A-03 every tab of the window carries a workspaceId",
    );
    const state = file.workspaceState;
    assert(state, "A-03 workspaceState exists");
    const wsIds = state.workspaces.map((ws) => ws.workspaceId);
    assertEquals(new Set(state.order).size, state.order.length, "unique order");
    assertEquals(state.order.length, wsIds.length, "order covers workspaces");
    assert(state.order.every((id) => wsIds.includes(id)), "order permutation");
    for (const id of ids) {
      assert(state.order.includes(id as TWorkspaceID), `order has ${id}`);
    }
    // A-05
    assertEquals(state.defaultID, store.defaultID, "A-05 defaultID");
    assertEquals(
      state.currentWorkspaceId,
      ids[2],
      "A-05 currentWorkspaceId is the shown workspace",
    );
    assert(state.currentWorkspaceId !== state.defaultID, "A-05 non-default");
  });
}

// R-01 regression: a backup triggered by a non-focused window still records
// the focused window's workspace as the file's current workspace.
async function testFocusedWindowWorkspaceIsCaptured(): Promise<void> {
  await withBackupCleanup(() =>
    withWindows(false, false, async (open) => {
      const focused = (await open()) as BackupTestWindow;
      const background = (await open()) as BackupTestWindow;
      const focusedApi = api(focused);
      const backgroundApi = api(background);
      const focusedWorkspace = focusedApi.workspaces.createNoNameWorkspace();
      await settle();
      backgroundApi.workspaces.createNoNameWorkspace();
      await settle();
      urlTab(focused, [`${PREFIX}/r01-focused`], 1);
      urlTab(background, [`${PREFIX}/r01-background`], 1);
      await settle();
      focused.focus();
      await waitFor(
        () => Services.wm.getMostRecentWindow("navigator:browser") === focused,
        "focused window becomes the most recent browser window",
      );

      const summary = await backgroundApi.backup.backupNow();
      const file = await readBackup(summary.fileName);
      assertEquals(
        file.workspaceState?.currentWorkspaceId,
        focusedWorkspace,
        "currentWorkspaceId comes from the focused window, not the capturing window",
      );
    })
  );
}

// A-04
async function testWorkspaceMetadata(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup, workspaces } = api(win);
    const original = workspaces.getSelectedWorkspaceID();
    const id = workspaces.createNoNameWorkspace();
    await settle();
    workspaces.changeWorkspace(original);
    await settle();
    const store = readStore();
    store.data = store.data.map(([key, value]) =>
      key === id
        ? [key, {
          ...value,
          name: "QA作業",
          icon: "briefcase",
          userContextId: 2,
        }]
        : [key, value]
    );
    writeStore(store);
    await settle();

    const file = await readBackup((await backup.backupNow()).fileName);
    const ws = file.workspaceState?.workspaces.find((entry) =>
      entry.workspaceId === id
    );
    assert(ws, "A-04 workspace is in the file");
    assertEquals(ws.name, "QA作業", "A-04 name");
    assertEquals(ws.icon, "briefcase", "A-04 icon");
    assertEquals(ws.userContextId, 2, "A-04 userContextId");
  });
}

// A-06 / A-07 / A-08 / A-09
async function testTabProperties(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    const pinA = urlTab(win, [`${PREFIX}/a06-pin-a`]);
    const pinB = urlTab(win, [`${PREFIX}/a06-pin-b`]);
    win.gBrowser.pinTab(pinA);
    win.gBrowser.pinTab(pinB);
    urlTab(win, [`${PREFIX}/a08-1`, `${PREFIX}/a08-2`, `${PREFIX}/a08-3`], 2);
    urlTab(win, [`${PREFIX}/a09-personal`], 1, 1);
    urlTab(win, [`${PREFIX}/a09-work`], 1, 2);
    // Tab order is now: pinA, pinB, initial tab, a08, personal, work.
    const third = win.gBrowser.tabs[2] as XULElement;
    win.gBrowser.selectedTab = third;
    await settle();
    const file = await readBackup((await backup.backupNow()).fileName);
    for (const url of [`${PREFIX}/a06-pin-a`, `${PREFIX}/a06-pin-b`]) {
      const tab = oneTab(file, url);
      assertEquals(tab.pinned, true, `A-06 ${url} pinned`);
      assertEquals(tab.state?.pinned, true, `A-06 ${url} state.pinned`);
    }
    assertEquals(
      oneTab(file, `${PREFIX}/a08-1`).pinned,
      false,
      "A-06 unpinned tab stays unpinned",
    );
    const window = windowWithUrl(file, `${PREFIX}/a08-1`);
    const selected = window.tabs.filter((tab) => tab.isSelected);
    assertEquals(selected.length, 1, "A-07 exactly one selected tab");
    assertEquals(window.tabs[2], selected[0], "A-07 third tab is selected");
    assertEquals(
      window.tabs.length,
      win.gBrowser.tabs.length,
      "A-07 every tab is captured, so positions match",
    );
    const history = oneTab(file, `${PREFIX}/a08-1`);
    assertEquals(entryUrls(history).length, 3, "A-08 three entries");
    assertEquals(history.state?.index, 2, "A-08 index=2");
    assertEquals(history.url, `${PREFIX}/a08-2`, "A-08 url is current entry");
    assertEquals(
      oneTab(file, `${PREFIX}/a09-personal`).userContextId,
      1,
      "A-09 personal container",
    );
    const work = oneTab(file, `${PREFIX}/a09-work`);
    assertEquals(work.userContextId, 2, "A-09 work container");
    assertEquals(work.state?.userContextId, 2, "A-09 state.userContextId");
  });
}

// A-11 / A-12 / A-16 / A-17 / A-18 / A-19 / A-21
async function testUrlAllowlist(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    const blank = win.gBrowser.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
      inBackground: true,
    }) as XULElement;
    blank.setAttribute("label", `${MARK}-blank`);
    urlTab(win, ["about:newtab"]);
    urlTab(win, ["about:home"]);
    const refused: Record<string, string> = {
      "A-16 about:config": `about:config?${MARK}`,
      "A-16 about:preferences": `about:preferences#${MARK}`,
      "A-17 file": `file:///tmp/${MARK}.html`,
      "A-18 about:hub": `about:hub?${MARK}`,
      "A-19 view-source": `view-source:https://example.com/${MARK}`,
      "A-19 data": `data:text/html,${MARK}`,
      "A-19 javascript": `javascript:void('${MARK}')`,
    };
    let refusedTabs = 0;
    for (const url of Object.values(refused)) {
      try {
        urlTab(win, [url]);
        refusedTabs++;
      } catch (error) {
        console.log("[qa-g1] setTabState refused", url, String(error));
      }
    }
    // A-21: whole history unrestorable.
    urlTab(win, [`file:///tmp/${MARK}-1.html`, `file:///tmp/${MARK}-2.html`]);
    refusedTabs++;
    await settle();

    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    const text = JSON.stringify(file);
    assert(
      !text.includes(MARK),
      `A-16..A-19/A-21 refused URLs are absent: ${
        text.slice(
          Math.max(0, text.indexOf(MARK) - 200),
          text.indexOf(MARK) + 50,
        )
      }`,
    );
    const window = windowWithUrl(file, "about:home");
    assertEquals(
      window.tabs.length,
      win.gBrowser.tabs.length - refusedTabs,
      "only refused tabs are left out",
    );
    const blankTab = window.tabs.find((tab) => tab.title === `${MARK}-blank`);
    // The title would contain MARK, so it is checked by position instead.
    assert(!blankTab, "label check is done through the window count");
    assert(
      window.tabs.some((tab) => tab.url === "about:blank"),
      "A-11 about:blank tab is saved",
    );
    assert(
      window.tabs.some((tab) => tab.url === "about:newtab"),
      "A-12 about:newtab tab is saved",
    );
    assert(
      window.tabs.some((tab) => tab.url === "about:home"),
      "A-12 about:home tab is saved",
    );
  });
}

// A-20
async function testPartialHistory(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    urlTab(win, [
      `file:///tmp/a20-${RUN}.html`,
      `${PREFIX}/a20-1`,
      `${PREFIX}/a20-2`,
    ], 3);
    await settle();
    const file = await readBackup((await backup.backupNow()).fileName);
    const tab = oneTab(file, `${PREFIX}/a20-1`);
    assertEquals(
      JSON.stringify(entryUrls(tab)),
      JSON.stringify([`${PREFIX}/a20-1`, `${PREFIX}/a20-2`]),
      "A-20 only the file entry is dropped",
    );
    assertEquals(tab.state?.index, 2, "A-20 index is remapped 3 -> 2");
    assertEquals(tab.url, `${PREFIX}/a20-2`, "A-20 url");
  });
}

// A-22 / A-23
async function testSensitiveStateDropped(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    const principal = Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI(`${PREFIX}/a23`),
      {},
    );
    const { E10SUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/E10SUtils.sys.mjs",
    ) as { E10SUtils: { serializePrincipal(principal: unknown): string } };
    const tab = stateTab(win, {
      entries: [{
        url: `${PREFIX}/a22`,
        title: "a22",
        triggeringPrincipal_base64: E10SUtils.serializePrincipal(principal),
        principalToInherit_base64: E10SUtils.serializePrincipal(principal),
        csp: `csp-${MARK}`,
        postdata_b64: btoa(`post-${MARK}`),
        referrerInfo: `ref-${MARK}`,
        structuredCloneState: btoa(`scs-${MARK}`),
      }],
      index: 1,
      formdata: { id: { field: `form-${MARK}` }, url: `${PREFIX}/a22` },
      scroll: { scroll: "0,500" },
      userTypedValue: `typed-${MARK}`,
      userTypedClear: 1,
      storage: { "https://example.com": { key: `storage-${MARK}` } },
    });
    await settle();
    const raw = rawState(tab);
    console.log(
      "[qa-g1] A-22/A-23 raw keys",
      JSON.stringify(Object.keys(raw)),
      JSON.stringify(Object.keys((raw.entries as object[])[0] ?? {})),
    );

    const file = await readBackup((await backup.backupNow()).fileName);
    const captured = oneTab(file, `${PREFIX}/a22`);
    const text = JSON.stringify(captured);
    assert(!text.includes(MARK), `A-22/A-23 no sensitive value: ${text}`);
    const state = captured.state ?? {};
    for (
      const key of [
        "formdata",
        "scroll",
        "userTypedValue",
        "userTypedClear",
        "storage",
      ]
    ) {
      assert(!(key in state), `A-22 ${key} is dropped`);
    }
    const entry = (state.entries as Record<string, unknown>[])[0];
    for (
      const key of [
        "postdata_b64",
        "triggeringPrincipal_base64",
        "principalToInherit_base64",
        "csp",
        "referrerInfo",
        "structuredCloneState",
      ]
    ) {
      assert(!(key in entry), `A-23 ${key} is dropped`);
    }
  });
}

// A-24 / A-25
async function testTabIcons(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    const dataIcon =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
    const dataTab = stateTab(win, {
      entries: [{ url: `${PREFIX}/a24`, title: "a24" }],
      index: 1,
      image: dataIcon,
    });
    const remoteTab = stateTab(win, {
      entries: [{ url: `${PREFIX}/a25`, title: "a25" }],
      index: 1,
      image: "https://example.com/favicon.ico",
    });
    await settle();
    assertEquals(rawState(dataTab).image, dataIcon, "A-24 precondition");
    console.log("[qa-g1] A-25 raw image", String(rawState(remoteTab).image));

    const file = await readBackup((await backup.backupNow()).fileName);
    assertEquals(
      oneTab(file, `${PREFIX}/a24`).state?.image,
      dataIcon,
      "A-24 data: icon is saved",
    );
    assert(
      !("image" in (oneTab(file, `${PREFIX}/a25`).state ?? {})),
      "A-25 remote icon is not saved",
    );
  });
}

// A-26 / A-27
async function testMarkersAndHidden(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup, workspaces } = api(win);
    const original = workspaces.getSelectedWorkspaceID();
    const visible = urlTab(win, [`${PREFIX}/a26`]);
    workspaces.createNoNameWorkspace();
    await settle();
    const hidden = urlTab(win, [`${PREFIX}/a27`]);
    await settle();
    workspaces.changeWorkspace(original);
    await settle();
    assert(hidden.hasAttribute("hidden"), "A-27 precondition: tab hidden");
    const rawVisible = SessionStore.getTabState(visible);
    console.log(
      "[qa-g1] A-26 raw has marker",
      rawVisible.includes(WORKSPACE_TAB_ATTRIBUTION_ID),
      "A-27 raw hidden",
      String(rawState(hidden).hidden),
    );

    const file = await readBackup((await backup.backupNow()).fileName);
    for (const url of [`${PREFIX}/a26`, `${PREFIX}/a27`]) {
      const stateText = JSON.stringify(oneTab(file, url).state);
      for (
        const marker of [
          "floorpWorkspaceId",
          "floorpWorkspaceLastShowId",
          "floorpLastShowWorkspaceId",
          "floorpDisableHistory",
        ]
      ) {
        assert(!stateText.includes(marker), `A-26 ${marker} removed (${url})`);
      }
    }
    const hiddenTab = oneTab(file, `${PREFIX}/a27`);
    assert(!("hidden" in (hiddenTab.state ?? {})), "A-27 hidden not saved");
    assert(hiddenTab.workspaceId, "A-27 hidden tab keeps its workspaceId");
  });
}

// A-29 / A-30
async function testStoreRepair(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup, workspaces } = api(win);
    const original = workspaces.getSelectedWorkspaceID();
    const second = workspaces.createNoNameWorkspace();
    await settle();
    workspaces.changeWorkspace(original);
    await settle();
    const store = readStore();
    const template = store.data.find(([key]) => key === second)?.[1];
    assert(template, "template workspace exists");
    const invalidKey = `invalid-${MARK}`;
    // Duplicate the first entry and drop `second` from order.
    store.order = [
      ...store.order.filter((id) => id !== second),
      store.order[0],
    ];
    store.data.push([invalidKey, { ...template, name: `bad-${MARK}` }]);
    writeStore(store);
    await settle();
    const live = readStore();
    console.log(
      "[qa-g1] A-29/A-30 live store after write",
      JSON.stringify(live.order.length),
      live.data.some(([key]) => key === invalidKey),
    );

    const summary = await backup.backupNow();
    const file = await readBackup(summary.fileName);
    assert(isWorkspaceBackupFile(file), "A-29 file validates");
    const state = file.workspaceState;
    assert(state, "workspace state exists");
    const ids = state.workspaces.map((ws) => ws.workspaceId as string);
    assertEquals(new Set(state.order).size, state.order.length, "A-29 unique");
    assertEquals(state.order.length, ids.length, "A-29 complete order");
    assert(ids.includes(second), "A-29 workspace missing from order is kept");
    assert(!ids.includes(invalidKey), "A-30 invalid key is excluded");
    assert(!JSON.stringify(file).includes(MARK), "A-30 invalid entry absent");
    const listed = await backup.listBackups();
    assert(
      listed.some((entry) => entry.fileName === summary.fileName),
      "A-29 repaired file is listed",
    );
  });
}

// A-31
async function testDanglingAttribution(): Promise<void> {
  await withWorkspaceWindow(async (win) => {
    const { backup } = api(win);
    const tab = urlTab(win, [`${PREFIX}/a31`]);
    await settle();
    const deleted = "11111111-1111-4111-8111-111111111111";
    tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, deleted);
    const file = await readBackup((await backup.backupNow()).fileName);
    const captured = oneTab(file, `${PREFIX}/a31`);
    assertEquals(captured.workspaceId, undefined, "A-31 workspaceId removed");
    assert(!JSON.stringify(file).includes(deleted), "A-31 id absent");
  });
}

// A-32
async function testWorkspaceStateUnavailable(): Promise<void> {
  await withBackupCleanup(async () => {
    const directory = PathUtils.join(
      PathUtils.profileDir,
      "workspaces",
      "backup",
    );
    const before = new Set(
      await IOUtils.getChildren(directory).catch(() => []),
    );
    const host: WorkspacesBackupHost = {
      supportsWorkspaceRestore: false,
      isWorkspaceID: (id: string): id is TWorkspaceID => false && id !== "",
      getRawWorkspace: (_id: TWorkspaceID): TWorkspace | undefined => undefined,
      restoreWorkspaceState: () => Promise.reject(new Error("unused")),
    };
    let service: WorkspacesBackupService | null = null;
    const dispose = createRoot((disposeRoot) => {
      service = new WorkspacesBackupService(host);
      return disposeRoot;
    });
    try {
      assert(service, "service constructed");
      const active = service as WorkspacesBackupService;
      active.dispose();
      let message = "";
      try {
        await active.backupNow("manual");
      } catch (error) {
        message = error instanceof Error ? error.message : String(error);
      }
      assertEquals(
        message,
        "workspace-state-unavailable",
        "A-32 capture fails closed",
      );
      const after = await IOUtils.getChildren(directory).catch(() => []);
      assertEquals(
        after.filter((path) => !before.has(path)).length,
        0,
        "A-32 no file is written",
      );
    } finally {
      dispose();
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG1Capture.test.ts", [
    { name: "A-03/A-05 workspace attribution", fn: testWorkspaceAttribution },
    { name: "A-04 workspace metadata", fn: testWorkspaceMetadata },
    {
      name: "R-01 focused window workspace capture",
      fn: testFocusedWindowWorkspaceIsCaptured,
    },
    { name: "A-06..A-09 tab properties", fn: testTabProperties },
    { name: "A-11/12/16..19/21 URL allowlist", fn: testUrlAllowlist },
    { name: "A-20 partial history", fn: testPartialHistory },
    { name: "A-22/A-23 sensitive state", fn: testSensitiveStateDropped },
    { name: "A-24/A-25 tab icons", fn: testTabIcons },
    { name: "A-26/A-27 markers and hidden", fn: testMarkersAndHidden },
    { name: "A-29/A-30 store repair", fn: testStoreRepair },
    { name: "A-31 dangling attribution", fn: testDanglingAttribution },
    {
      name: "A-32 workspace state unavailable",
      fn: testWorkspaceStateUnavailable,
    },
  ]);
}
