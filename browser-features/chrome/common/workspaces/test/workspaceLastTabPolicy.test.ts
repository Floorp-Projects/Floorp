// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_PENDING_EXIT_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
  WORKSPACED_CONFIG_PREF_NAME,
} from "../utils/workspaces-static-names.ts";

const CLOSE_PREF = "browser.tabs.closeWindowWithLastTab";

type TestWindow = Window & {
  gBrowser: Omit<GBrowser, "removeTab"> & {
    removeTab(tab: XULElement, options: { animate: boolean }): void;
  };
  workspacesFuncs?: {
    createNoNameWorkspace(): string;
    getSelectedWorkspaceID(): string;
    changeWorkspace(id: string): void;
  };
};

async function waitFor(
  predicate: () => boolean,
  message: string,
): Promise<void> {
  const deadline = Date.now() + 15_000;
  while (!predicate() && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  assert(predicate(), message);
}

async function settle(): Promise<void> {
  // Window closure is deliberately deferred by the workspace manager.
  await new Promise((resolve) => setTimeout(resolve, 250));
}

async function withWindows(
  closeWithLastTab: boolean,
  exitWorkspace: boolean,
  check: (open: () => Promise<TestWindow>) => Promise<void>,
): Promise<void> {
  const windows: TestWindow[] = [];
  const previousClose = Services.prefs.getBoolPref(CLOSE_PREF);
  const hadClose = Services.prefs.prefHasUserValue(CLOSE_PREF);
  const previousConfig = Services.prefs.getStringPref(
    WORKSPACED_CONFIG_PREF_NAME,
  );
  const previousData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const previousPendingExit = Services.prefs.getBoolPref(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
    false,
  );
  const hadPendingExit = Services.prefs.prefHasUserValue(
    WORKSPACE_PENDING_EXIT_PREF_NAME,
  );
  try {
    Services.prefs.setBoolPref(CLOSE_PREF, closeWithLastTab);
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify({
        ...JSON.parse(previousConfig),
        exitOnLastTabClose: exitWorkspace,
      }),
    );
    await check(async () => {
      const win = (globalThis as unknown as {
        OpenBrowserWindow(): TestWindow;
      }).OpenBrowserWindow();
      windows.push(win);
      await waitFor(
        () =>
          Boolean(
            win.workspacesFuncs &&
              win.gBrowser?.selectedTab?.hasAttribute(
                WORKSPACE_TAB_ATTRIBUTION_ID,
              ),
          ),
        "test window initializes its actual workspace manager",
      );
      await settle();
      assertEquals(
        win.gBrowser.tabs.length,
        1,
        "test window starts with one tab",
      );
      assertEquals(
        Services.prefs.getBoolPref(CLOSE_PREF),
        closeWithLastTab,
        "opening a window preserves the native preference",
      );
      return win;
    });
    assertEquals(
      Services.prefs.getBoolPref(CLOSE_PREF),
      closeWithLastTab,
      "tab and workspace changes preserve the native preference",
    );
  } finally {
    for (const win of windows) {
      if (!win.closed) win.close();
    }
    await waitFor(
      () => windows.every((win) => win.closed),
      "test windows close",
    );
    await settle();
    const actualClose = Services.prefs.getBoolPref(CLOSE_PREF);
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, previousData);
    Services.prefs.setStringPref(WORKSPACED_CONFIG_PREF_NAME, previousConfig);
    if (hadClose) Services.prefs.setBoolPref(CLOSE_PREF, previousClose);
    else Services.prefs.clearUserPref(CLOSE_PREF);
    if (hadPendingExit) {
      Services.prefs.setBoolPref(
        WORKSPACE_PENDING_EXIT_PREF_NAME,
        previousPendingExit,
      );
    } else Services.prefs.clearUserPref(WORKSPACE_PENDING_EXIT_PREF_NAME);
    assertEquals(
      actualClose,
      closeWithLastTab,
      "closing windows preserves the native preference",
    );
  }
}

function addHiddenWorkspace(win: TestWindow): XULElement {
  const funcs = win.workspacesFuncs;
  assert(funcs, "workspace API is available");
  const original = funcs.getSelectedWorkspaceID();
  funcs.createNoNameWorkspace();
  const hidden = win.gBrowser.selectedTab as XULElement;
  funcs.changeWorkspace(original);
  assert(hidden.hasAttribute("hidden"), "other workspace tab is hidden");
  return hidden;
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceLastTabPolicy.test.ts", [
    ...[false, true].map((exitWorkspace) => ({
      name:
        `native false keeps the last tab's window open (workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(false, exitWorkspace, async (open) => {
          const win = await open();
          const closing = win.gBrowser.selectedTab;
          win.gBrowser.removeTab(closing, { animate: false });
          await settle();
          assert(!win.closed, "explicit false keeps the window open");
          assertEquals(
            win.gBrowser.tabs.length,
            1,
            "exactly one replacement remains",
          );
          assert(
            win.gBrowser.selectedTab !== closing,
            "the old tab was removed",
          );
        }),
    })),
    ...[false, true].map((exitWorkspace) => ({
      name:
        `native true closes an ordinary window even with hidden tabs in another (workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(true, exitWorkspace, async (open) => {
          const other = await open();
          const hidden = addHiddenWorkspace(other);
          const win = await open();
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await waitFor(
            () => win.closed,
            "ordinary last-tab window closes (#2684)",
          );
          assert(!other.closed, "other window stays open");
          assert(
            other.gBrowser.tabs.includes(hidden),
            "other window's hidden tab survives",
          );
        }),
    })),
    ...[
      { closeWithLastTab: false, exitWorkspace: false },
      { closeWithLastTab: false, exitWorkspace: true },
      { closeWithLastTab: true, exitWorkspace: false },
    ].map(({ closeWithLastTab, exitWorkspace }) => ({
      name:
        `last visible tab switches to preserved workspace (native ${closeWithLastTab}, workspace exit ${exitWorkspace})`,
      fn: () =>
        withWindows(closeWithLastTab, exitWorkspace, async (open) => {
          const win = await open();
          const hidden = addHiddenWorkspace(win);
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await settle();
          assert(
            !win.closed,
            "native false or workspace exit false prevents closure",
          );
          assert(
            win.gBrowser.tabs.includes(hidden),
            "hidden user tab survives",
          );
          assertEquals(
            win.gBrowser.selectedTab,
            hidden,
            "remaining workspace becomes selected",
          );
          assert(
            !hidden.hasAttribute("hidden"),
            "remaining workspace becomes visible",
          );
        }),
    })),
    {
      name:
        "workspace exit preserves hidden tabs in the closed window's session",
      fn: () =>
        withWindows(true, true, async (open) => {
          const win = await open();
          const hidden = addHiddenWorkspace(win);
          const hiddenWorkspace = hidden.getAttribute(
            WORKSPACE_TAB_ATTRIBUTION_ID,
          );
          assert(hiddenWorkspace, "hidden tab belongs to a workspace");
          // Blank-only windows are intentionally omitted from closed-window data.
          win.gBrowser.getBrowserForTab(hidden).loadURI(
            Services.io.newURI("about:robots"),
            {
              triggeringPrincipal: Services.scriptSecurityManager
                .getSystemPrincipal(),
            },
          );
          await waitFor(
            () =>
              win.gBrowser.getBrowserForTab(hidden).currentURI?.spec ===
                "about:robots",
            "hidden tab has restorable content",
          );
          await settle();
          win.gBrowser.removeTab(win.gBrowser.selectedTab, { animate: false });
          await waitFor(() => win.closed, "workspace exit closes the window");
          const sessionStore = (globalThis as unknown as {
            SessionStore: {
              getClosedWindowData(): {
                tabs: {
                  floorpWorkspaceId?: string;
                  entries?: { url?: string }[];
                }[];
              }[];
            };
          }).SessionStore;
          await waitFor(
            () =>
              sessionStore.getClosedWindowData().some((closed) =>
                closed.tabs.some((tab) =>
                  tab.floorpWorkspaceId === hiddenWorkspace &&
                  tab.entries?.some((entry) => entry.url === "about:robots")
                )
              ),
            "hidden tab URL and workspace are included in the closed-window session",
          );
        }),
    },
  ]);
}
