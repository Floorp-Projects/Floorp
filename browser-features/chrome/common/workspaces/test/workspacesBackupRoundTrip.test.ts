// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
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
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type BackupTestWindow = TestWindow & {
  workspacesBackupFuncs?: WorkspacesBackupFuncs;
  workspacesFuncs?: TestWindow["workspacesFuncs"] & {
    isWorkspaceID(id: string): boolean;
  };
};

interface SessionStoreApi {
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
}

interface TabHistory {
  urls: string[];
  index: number;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: SessionStoreApi };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as { E10SUtils: { serializePrincipal(principal: unknown): string } };

const PREFIX = `https://example.com/floorp-backup-round-trip/${Date.now()}`;

function backupErrors(): string {
  const storage =
    (Cc as Record<string, { getService(iface: unknown): unknown }>)[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage) as {
      getEvents(): { level?: string; arguments?: unknown[] }[];
    };
  return storage.getEvents()
    .filter((event) => event.level === "error")
    .map((event) =>
      (event.arguments ?? []).map((arg) =>
        arg instanceof Error ? `${arg.message}` : String(arg)
      ).join(" ")
    )
    .filter((text) => /Workspaces/.test(text))
    .slice(-10)
    .join("\n");
}

function readHistory(tab: XULElement): TabHistory {
  const state = JSON.parse(SessionStore.getTabState(tab)) as {
    entries?: { url?: string }[];
    index?: number;
  };
  return {
    urls: (state.entries ?? []).map((entry) => entry.url ?? ""),
    index: state.index ?? 0,
  };
}

function openTab(
  win: BackupTestWindow,
  history: TabHistory,
  userContextId = 0,
): XULElement {
  const tab = win.gBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    inBackground: true,
    userContextId,
  }) as XULElement;
  SessionStore.setTabState(
    tab,
    JSON.stringify({
      entries: history.urls.map((url) => ({
        url,
        triggeringPrincipal_base64: E10SUtils.serializePrincipal(
          Services.scriptSecurityManager.createContentPrincipal(
            Services.io.newURI(url),
            { userContextId },
          ),
        ),
      })),
      index: history.index,
      userContextId,
    }),
  );
  return tab;
}

function browserWindows(): BackupTestWindow[] {
  const windows: BackupTestWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as BackupTestWindow;
    if (!win.closed) windows.push(win);
  }
  return windows;
}

function findTabs(win: BackupTestWindow, url: string): XULElement[] {
  return [...win.gBrowser.tabs].filter((tab) =>
    readHistory(tab as XULElement).urls.includes(url)
  ) as XULElement[];
}

function findOneTab(win: BackupTestWindow, url: string): XULElement {
  const tabs = findTabs(win, url);
  assertEquals(tabs.length, 1, `exactly one restored tab holds ${url}`);
  return tabs[0];
}

function assertHistory(tab: XULElement, expected: TabHistory, label: string) {
  const actual = readHistory(tab);
  assertEquals(
    JSON.stringify(actual.urls),
    JSON.stringify(expected.urls),
    `${label}: history entries are restored`,
  );
  assertEquals(actual.index, expected.index, `${label}: history index`);
}

function removeTabs(win: BackupTestWindow, tabs: XULElement[]): void {
  for (const tab of tabs) {
    win.gBrowser.removeTab(tab, { animate: false });
  }
}

/**
 * The capture spans every browser window, so a restore also rewrites the
 * workspace attribution of the harness window's tabs and leaves new backup
 * files behind. Undo both so later tests start from the same state.
 */
async function withBackupCleanup(
  check: (created: () => Promise<string[]>) => Promise<void>,
): Promise<void> {
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
  const funcs = (harnessWindows[0] as BackupTestWindow | undefined)
    ?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set(
    (await funcs.listBackups()).map((backup) => backup.fileName),
  );
  const created = async () =>
    (await funcs.listBackups())
      .map((backup) => backup.fileName)
      .filter((fileName) => !before.has(fileName));
  try {
    await check(created);
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const fileName of await created()) {
      await funcs.deleteBackup(fileName);
    }
    for (const [tab, value] of attributions) {
      if (!tab.isConnected) continue;
      if (value === null) tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

async function testWorkspacesModeRoundTrip(): Promise<void> {
  await withBackupCleanup((created) =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      const workspaces = win.workspacesFuncs;
      assert(backup && workspaces, "backup and workspace APIs are available");

      const historyA: TabHistory = {
        urls: [`${PREFIX}/a-1`, `${PREFIX}/a-2`, `${PREFIX}/a-3`],
        index: 2,
      };
      const pinnedA: TabHistory = { urls: [`${PREFIX}/a-pinned`], index: 1 };
      const containerB: TabHistory = {
        urls: [`${PREFIX}/b-1`, `${PREFIX}/b-2`],
        index: 2,
      };
      const historyTab = openTab(win, historyA);
      const pinnedTab = openTab(win, pinnedA);
      win.gBrowser.pinTab(pinnedTab);
      workspaces.createNoNameWorkspace();
      await settle();
      const containerTab = openTab(win, containerB, 1);
      win.gBrowser.selectedTab = containerTab;
      await settle();
      const originalA = historyTab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      const originalB = containerTab.getAttribute(
        WORKSPACE_TAB_ATTRIBUTION_ID,
      );
      assert(
        originalA && originalB && originalA !== originalB,
        "setup spreads tabs over two workspaces",
      );

      const summary = await backup.backupNow();
      assertEquals(summary.mode, "workspaces", "capture uses workspaces mode");

      // Lose the data the backup should bring back.
      workspaces.changeWorkspace(originalA);
      await settle();
      removeTabs(win, [containerTab, pinnedTab]);
      SessionStore.setTabState(
        historyTab,
        JSON.stringify({ entries: [{ url: "about:blank" }], index: 1 }),
      );
      await settle();

      const result = await backup.restoreBackup(summary.fileName);
      assert(
        result.success,
        `restore succeeds: ${JSON.stringify(result)}\n${backupErrors()}`,
      );
      await settle();

      const restoredHistory = findOneTab(win, historyA.urls[0]);
      const restoredPinned = findOneTab(win, pinnedA.urls[0]);
      const restoredContainer = findOneTab(win, containerB.urls[0]);
      assertHistory(restoredHistory, historyA, "history tab");
      assertHistory(restoredPinned, pinnedA, "pinned tab");
      assertHistory(restoredContainer, containerB, "container tab");

      assert(restoredPinned.hasAttribute("pinned"), "pinned tab stays pinned");
      assert(
        !restoredHistory.hasAttribute("pinned"),
        "unpinned tab stays unpinned",
      );
      assertEquals(
        restoredContainer.getAttribute("usercontextid"),
        "1",
        "container tab keeps its container",
      );

      const restoredA = restoredHistory.getAttribute(
        WORKSPACE_TAB_ATTRIBUTION_ID,
      );
      const restoredB = restoredContainer.getAttribute(
        WORKSPACE_TAB_ATTRIBUTION_ID,
      );
      assert(
        restoredA && workspaces.isWorkspaceID(restoredA),
        "history tab belongs to a restored workspace",
      );
      assert(
        restoredB && workspaces.isWorkspaceID(restoredB),
        "container tab belongs to a restored workspace",
      );
      assertEquals(
        restoredPinned.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
        restoredA,
        "tabs of one workspace stay together",
      );
      assert(restoredA !== restoredB, "workspaces stay separate");
      assert(
        !workspaces.isWorkspaceID(originalA) &&
          !workspaces.isWorkspaceID(originalB),
        "restore replaces the pre-restore workspaces",
      );
      assertEquals(
        workspaces.getSelectedWorkspaceID(),
        restoredB,
        "the workspace that was current at capture is selected",
      );
      assertEquals(
        win.gBrowser.selectedTab,
        restoredContainer,
        "the tab that was selected at capture is selected",
      );

      const kinds = (await backup.listBackups())
        .filter((entry) => entry.fileName !== summary.fileName)
        .filter((entry) => entry.kind === "pre-restore");
      assert(kinds.length > 0, "a pre-restore safety copy was written");
      assert((await created()).length >= 2, "backup files were created");
    })
  );
}

async function testTabsModeRoundTrip(): Promise<void> {
  await withBackupCleanup((_created) =>
    withWindows(false, false, async (open) => {
      const win = (await open()) as BackupTestWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API is available while workspaces are disabled");
      assert(!win.workspacesFuncs, "workspaces are disabled");

      const history: TabHistory = {
        urls: [`${PREFIX}/t-1`, `${PREFIX}/t-2`],
        index: 1,
      };
      const pinned: TabHistory = { urls: [`${PREFIX}/t-pinned`], index: 1 };
      const container: TabHistory = {
        urls: [`${PREFIX}/t-container`],
        index: 1,
      };
      const historyTab = openTab(win, history);
      const pinnedTab = openTab(win, pinned);
      win.gBrowser.pinTab(pinnedTab);
      const containerTab = openTab(win, container, 2);
      win.focus();
      await settle();

      const summary = await backup.backupNow();
      assertEquals(summary.mode, "tabs", "capture uses tabs mode");

      removeTabs(win, [historyTab, pinnedTab, containerTab]);
      await settle();

      const result = await backup.restoreBackup(summary.fileName);
      assert(
        result.success,
        `restore succeeds: ${JSON.stringify(result)}\n${backupErrors()}`,
      );
      await waitFor(
        () => findTabs(win, container.urls[0]).length > 0,
        "restored tabs appear in the current window",
      );
      await settle();

      const restoredHistory = findOneTab(win, history.urls[0]);
      const restoredPinned = findOneTab(win, pinned.urls[0]);
      const restoredContainer = findOneTab(win, container.urls[0]);
      assertHistory(restoredHistory, history, "history tab");
      assertHistory(restoredPinned, pinned, "pinned tab");
      assertHistory(restoredContainer, container, "container tab");
      assert(restoredPinned.hasAttribute("pinned"), "pinned tab stays pinned");
      assertEquals(
        restoredContainer.getAttribute("usercontextid"),
        "2",
        "container tab keeps its container",
      );
      for (const tab of [restoredHistory, restoredPinned, restoredContainer]) {
        assert(
          !tab.hasAttribute(WORKSPACE_TAB_ATTRIBUTION_ID),
          "tabs mode restores no workspace attribution",
        );
      }
    }, false)
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupRoundTrip.test.ts", [
    {
      name: "workspaces mode restores tabs, history and workspaces",
      fn: testWorkspacesModeRoundTrip,
    },
    {
      name: "tabs mode restores tabs and history without workspaces",
      fn: testTabsModeRoundTrip,
    },
  ]);
}
