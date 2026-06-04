import type { GestureActionRegistration } from "./gestures.ts";
import { setShareModeEnabled } from "#features-chrome/common/browser-share-mode/browser-share-mode.tsx";
import {
  setPersistedGroupLayout,
  getSplitViewGroupIdForTabs,
} from "#features-chrome/common/split-view/patches/session-restore.js";
import { applyLayoutAttribute } from "#features-chrome/common/split-view/layout.js";
import { updateHandles } from "#features-chrome/common/split-view/components/split-view-splitters.js";
import type { SplitViewLayout } from "#features-chrome/common/split-view/data/types.js";
import {
  toggleUserInterface,
  toggleNavigationPanel,
  enableRestMode,
} from "./ui-toggle.ts";

const getXulElement = (id: string, win?: Window): XULElement | null => {
  try {
    const targetDoc = win?.document ?? document;
    return (targetDoc?.getElementById(id) as XULElement | null) ?? null;
  } catch {
    return null;
  }
};

const doCommandIfExists = (id: string, win?: Window) => {
  const el = getXulElement(id, win);
  try {
    el?.doCommand?.();
  } catch (e) {
    console.error(`[mouse-gesture] Failed to run command for ${id}:`, e);
  }
};

const clickIfExists = (id: string, win?: Window) => {
  const el = getXulElement(id, win);
  try {
    el?.click?.();
  } catch (e) {
    console.error(`[mouse-gesture] Failed to click ${id}:`, e);
  }
};

const sendScrollCommand = (
  direction:
    | "lineUp"
    | "lineDown"
    | "pageUp"
    | "pageDown"
    | "lineRight"
    | "lineLeft"
    | "toTop"
    | "toBottom",
  win: Window,
) => {
  try {
    const browser = win.gBrowser?.selectedBrowser;
    if (!browser) return;
    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRMouseGestureScroll",
    );
    if (!actor) return;
    console.debug(
      "[mouse-gesture] Sending scroll command to content:",
      direction,
    );
    actor.sendAsyncMessage("MouseGestureScroll:Execute", { direction });
  } catch (e) {
    console.error("[mouse-gesture] Failed to send scroll command:", e);
  }
};

function applyThreePaneLayout(
  win: Window,
  targetLayout:
    | "grid-3pane-left-main"
    | "grid-3pane-right-main"
    | "grid-3pane-top-main"
    | "grid-3pane-bottom-main",
): void {
  const gBrowser = (
    win as unknown as {
      gBrowser: {
        activeSplitView: { tabs: unknown[] } | null;
        tabpanels: { splitViewPanels: unknown[] } | null;
      };
    }
  ).gBrowser;
  const activeSplitView = gBrowser?.activeSplitView;
  if (!activeSplitView) return;

  const groupId = getSplitViewGroupIdForTabs(
    activeSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
  );
  if (groupId) {
    setPersistedGroupLayout(groupId, targetLayout);
  }

  const logger = console.createInstance({ prefix: "[mouse-gesture:split-view]" });
  applyLayoutAttribute(logger, targetLayout, 3);
  const panels = gBrowser.tabpanels?.splitViewPanels as string[] | undefined;
  if (panels) {
    updateHandles(panels, targetLayout);
  }
}

export const actions: GestureActionRegistration[] = [
  {
    name: "gecko-back",
    fn: (win) => {
      const el = getXulElement("back-button", win);
      if (el) doCommandIfExists("back-button", win);
    },
  },
  {
    name: "gecko-forward",
    fn: (win) => {
      const el = getXulElement("forward-button", win);
      if (el) doCommandIfExists("forward-button", win);
    },
  },
  {
    name: "gecko-reload",
    fn: (win) => {
      const el = getXulElement("reload-button", win);
      if (el) doCommandIfExists("reload-button", win);
    },
  },
  {
    name: "gecko-close-tab",
    fn: (win) =>
      win.gBrowser.removeCurrentTab({
        animate: true,
        ...win.gBrowser.TabMetrics.userTriggeredContext(),
      }),
  },
  {
    name: "gecko-close-other-tabs",
    fn: (win) => {
      const currentTab = win.gBrowser.selectedTab;
      const tabsToClose = win.gBrowser.visibleTabs.filter(
        (tab: XULElement) => tab !== currentTab,
      );
      win.gBrowser.removeTabs(tabsToClose);
    },
  },
  {
    name: "gecko-close-tabs-to-start",
    fn: (win) => {
      win.gBrowser.removeTabsToTheStartFrom(
        win.gBrowser.selectedTab,
        win.gBrowser.TabMetrics.userTriggeredContext(),
      );
    },
  },
  {
    name: "gecko-close-tabs-to-end",
    fn: (win) => {
      win.gBrowser.removeTabsToTheEndFrom(
        win.gBrowser.selectedTab,
        win.gBrowser.TabMetrics.userTriggeredContext(),
      );
    },
  },
  {
    name: "gecko-open-new-tab",
    fn: (win) => win.BrowserCommands.openTab(),
  },
  {
    name: "gecko-duplicate-tab",
    fn: (win) => win.gBrowser.duplicateTab(win.gBrowser.selectedTab),
  },
  {
    name: "gecko-reload-all-tabs",
    fn: (win) => {
      if (typeof win.gBrowser?.reloadTabs === "function") {
        win.gBrowser.reloadTabs(win.gBrowser.tabs);
      } else if (typeof win.BrowserReload === "function") {
        for (const _tab of win.gBrowser.tabs) {
          win.BrowserReload();
        }
      }
    },
  },
  {
    name: "gecko-open-new-window",
    fn: (win) => win.OpenBrowserWindow(),
  },
  {
    name: "gecko-open-new-private-window",
    fn: (win) => win.OpenBrowserWindow({ private: true }),
  },
  {
    name: "gecko-close-window",
    fn: (win) => win.close(),
  },
  {
    name: "gecko-restore-last-window",
    fn: (win) => win.SessionWindowUI.undoCloseWindow(0),
  },
  {
    name: "gecko-show-next-tab",
    fn: (win) => win.gBrowser.tabContainer.advanceSelectedTab(1, true),
  },
  {
    name: "gecko-show-previous-tab",
    fn: (win) => win.gBrowser.tabContainer.advanceSelectedTab(-1, true),
  },
  {
    name: "gecko-show-previously-selected-tab",
    fn: (win) => {
      const currentTab = win.gBrowser.selectedTab;
      let latest = null;
      for (const tab of win.gBrowser.tabs) {
        if (tab._lastAccessed === Infinity) continue;
        // Skip the currently selected tab
        if (tab === currentTab) continue;
        if (!latest || tab._lastAccessed > latest._lastAccessed) latest = tab;
      }
      if (latest) {
        win.gBrowser.tabContainer._selectNewTab(latest);
      }
    },
  },
  {
    name: "gecko-show-all-tabs-panel",
    fn: (win) => win.gTabsPanel.showAllTabsPanel(),
  },
  {
    name: "gecko-force-reload",
    fn: (win) => {
      if (typeof win.BrowserCommands?.reloadSkipCache === "function") {
        win.BrowserCommands.reloadSkipCache();
      } else if (typeof win.BrowserReloadSkipCache === "function") {
        win.BrowserReloadSkipCache();
      }
    },
  },
  {
    name: "gecko-zoom-in",
    fn: (win) => win.FullZoom.enlarge(),
  },
  {
    name: "gecko-zoom-out",
    fn: (win) => win.FullZoom.reduce(),
  },
  {
    name: "gecko-reset-zoom",
    fn: (win) => win.FullZoom.reset(),
  },
  {
    name: "gecko-bookmark-this-page",
    fn: (win) => win.PlacesCommandHook.bookmarkPage(),
  },
  {
    name: "gecko-open-bookmark-add-tool",
    fn: (win) =>
      win.PlacesUIUtils.showBookmarkPagesDialog(
        win.PlacesCommandHook.uniqueCurrentPages,
      ),
  },
  {
    name: "gecko-open-bookmarks-manager",
    fn: (win) => win.SidebarController.toggle("viewBookmarksSidebar"),
  },
  {
    name: "gecko-toggle-bookmark-toolbar",
    fn: (win) => win.BookmarkingUI.toggleBookmarksToolbar("bookmark-tools"),
  },
  {
    name: "gecko-open-home-page",
    fn: (win) =>
      win.switchToTabHavingURI(
        Services.prefs.getStringPref("browser.startup.homepage"),
        true,
      ),
  },
  {
    name: "gecko-open-addons-manager",
    fn: (win) => {
      if (typeof win.BrowserAddonUI?.openAddonsMgr === "function") {
        win.BrowserAddonUI.openAddonsMgr();
      } else if (typeof win.BrowserOpenAddonsMgr === "function") {
        win.BrowserOpenAddonsMgr();
      }
    },
  },
  {
    name: "gecko-send-with-mail",
    fn: (win) =>
      win.MailIntegration.sendLinkForBrowser(win.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-save-page",
    fn: (win) => win.saveBrowser(win.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-print-page",
    fn: (win) =>
      win.PrintUtils.startPrintWindow(
        win.gBrowser.selectedBrowser.browsingContext,
      ),
  },
  {
    name: "gecko-mute-current-tab",
    fn: (win) =>
      win.gBrowser.toggleMuteAudioOnMultiSelectedTabs(win.gBrowser.selectedTab),
  },
  {
    name: "gecko-show-source-of-page",
    fn: (win) => win.BrowserViewSource(win.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-show-page-info",
    fn: (win) => {
      if (typeof win.BrowserCommands?.pageInfo === "function") {
        win.BrowserCommands.pageInfo();
      } else if (typeof win.BrowserPageInfo === "function") {
        win.BrowserPageInfo();
      }
    },
  },
  {
    name: "floorp-rest-mode",
    fn: (win) => enableRestMode(win),
  },
  {
    name: "floorp-hide-user-interface",
    fn: (win) => toggleUserInterface(win.document!),
  },
  {
    name: "floorp-toggle-navigation-panel",
    fn: (win) => toggleNavigationPanel(win.document!),
  },
  {
    name: "gecko-stop",
    fn: (win) => win.gBrowser.selectedBrowser.stop(),
  },
  {
    name: "gecko-search-in-this-page",
    fn: (win) => win.gLazyFindCommand("onFindCommand"),
  },
  {
    name: "gecko-show-next-search-result",
    fn: (win) => win.gLazyFindCommand("onFindAgainCommand", false),
  },
  {
    name: "gecko-show-previous-search-result",
    fn: (win) => win.gLazyFindCommand("onFindAgainCommand", true),
  },
  {
    name: "gecko-search-the-web",
    fn: (win) => win.BrowserSearch.webSearch(),
  },
  {
    name: "gecko-open-migration-wizard",
    fn: (win) =>
      win.MigrationUtils.showMigrationWizard(win, {
        entrypoint: win.MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
      }),
  },
  {
    name: "gecko-quit-from-application",
    fn: (win) => win.Services.startup.quit(win.Ci.nsIAppStartup.eForceQuit),
  },
  {
    name: "gecko-enter-into-customize-mode",
    fn: (win) => win.gCustomizeMode.enter(),
  },
  {
    name: "gecko-enter-into-offline-mode",
    fn: (win) => win.BrowserOffline.toggleOfflineStatus(),
  },
  {
    name: "gecko-open-screen-capture",
    fn: (win) => win.ScreenshotsUtils.start(win.gBrowser.selectedBrowser),
  },
  {
    name: "floorp-show-pip",
    fn: (win) => clickIfExists("picture-in-picture-button", win),
  },
  {
    name: "gecko-restore-last-tab",
    fn: (win) => win.SessionStore.undoCloseTab(win, 0),
  },
  {
    name: "gecko-open-general-preferences",
    fn: (win) => win.openPreferences(),
  },
  {
    name: "gecko-open-privacy-preferences",
    fn: (win) => win.openPreferences("panePrivacy"),
  },
  {
    name: "gecko-open-workspaces-preferences",
    fn: (win) => win.openPreferences("paneWorkspaces"),
  },
  {
    name: "gecko-open-containers-preferences",
    fn: (win) => win.openPreferences("paneContainers"),
  },
  {
    name: "gecko-open-search-preferences",
    fn: (win) => win.openPreferences("paneSearch"),
  },
  {
    name: "gecko-open-sync-preferences",
    fn: (win) => win.openPreferences("paneSync"),
  },
  {
    name: "gecko-open-task-manager",
    fn: (win) => win.switchToTabHavingURI("about:processes", true),
  },
  {
    name: "gecko-forget-history",
    fn: (win) => win.Sanitizer.showUI(win),
  },
  {
    name: "gecko-quick-forget-history",
    fn: (win) => win.PlacesUtils.history.clear(true),
  },
  {
    name: "gecko-restore-last-session",
    fn: (win) => win.SessionStore.restoreLastSession(),
  },
  {
    name: "gecko-search-history",
    fn: (win) => win.PlacesCommandHook.searchHistory(),
  },
  {
    name: "gecko-manage-history",
    fn: (win) => win.PlacesCommandHook.showPlacesOrganizer("History"),
  },
  {
    name: "gecko-open-downloads",
    fn: (win) => win.DownloadsPanel.showDownloadsHistory(),
  },
  {
    name: "gecko-show-bookmark-sidebar",
    fn: (win) => win.SidebarController.show("viewBookmarksSidebar"),
  },
  {
    name: "gecko-show-history-sidebar",
    fn: (win) => win.SidebarController.show("viewHistorySidebar"),
  },
  {
    name: "gecko-show-synced-tabs-sidebar",
    fn: (win) => win.SidebarController.show("viewTabsSidebar"),
  },
  {
    name: "gecko-reverse-sidebar",
    fn: (win) => win.SidebarController.reversePosition(),
  },
  {
    name: "gecko-hide-sidebar",
    fn: (win) => win.SidebarController.hide(),
  },
  {
    name: "gecko-toggle-sidebar",
    fn: (_win) => {
      // Toggle Floorp's Panel Sidebar, not Firefox's built-in SidebarController.
      // SidebarController.toggle() opens Firefox's native sidebar (History, etc.)
      // which is not what Floorp users expect for "Toggle Sidebar".
      const prefName = "floorp.panelSidebar.enabled";
      const current = Services.prefs.getBoolPref(prefName, true);
      Services.prefs.setBoolPref(prefName, !current);
    },
  },
  {
    name: "gecko-scroll-line-up",
    fn: (win) => sendScrollCommand("lineUp", win),
  },
  {
    name: "gecko-scroll-line-down",
    fn: (win) => sendScrollCommand("lineDown", win),
  },
  {
    name: "gecko-scroll-up",
    fn: (win) => sendScrollCommand("pageUp", win),
  },
  {
    name: "gecko-scroll-down",
    fn: (win) => sendScrollCommand("pageDown", win),
  },
  {
    name: "gecko-scroll-right",
    fn: (win) => sendScrollCommand("lineRight", win),
  },
  {
    name: "gecko-scroll-left",
    fn: (win) => sendScrollCommand("lineLeft", win),
  },
  {
    name: "gecko-scroll-to-top",
    fn: (win) => sendScrollCommand("toTop", win),
  },
  {
    name: "gecko-scroll-to-bottom",
    fn: (win) => sendScrollCommand("toBottom", win),
  },
  {
    name: "gecko-workspace-next",
    fn: (win) => win.workspacesFuncs.changeWorkspaceToNext(),
  },
  {
    name: "gecko-workspace-previous",
    fn: (win) => win.workspacesFuncs.changeWorkspaceToPrevious(),
  },
  {
    name: "floorp-toggle-zen-mode",
    fn: (_win) => {
      Services.prefs.setBoolPref(
        "floorp.zenmode.enabled",
        !Services.prefs.getBoolPref("floorp.zenmode.enabled", false),
      );
    },
  },
  {
    name: "floorp-open-settings",
    fn: (win) => {
      win.openPreferences();
    },
  },
  {
    name: "floorp-open-hub",
    fn: (win) => {
      win.switchToTabHavingURI(Services.io.newURI("about:hub"), true);
    },
  },
  {
    name: "floorp-toggle-share-mode",
    fn: () => setShareModeEnabled((prev) => !prev),
  },
  {
    name: "floorp-copy-page-url-as-markdown",
    fn: (win) => {
      const browser = win?.gBrowser?.selectedBrowser;
      if (!browser) {
        console.error("[copy-url-as-markdown] No selected browser found");
        return;
      }
      const url = browser.currentURI?.spec;
      if (!url) {
        console.error("[copy-url-as-markdown] Could not get current URI");
        return;
      }
      const title = browser.contentTitle || url;
      const escapedTitle = title.replace(/\[/g, "\\[").replace(/\]/g, "\\]");
      const markdown = `[${escapedTitle}](${url})`;
      if (!navigator?.clipboard?.writeText) {
        console.error("[copy-url-as-markdown] Clipboard API not available");
        return;
      }
      navigator.clipboard.writeText(markdown).catch((e) => {
        console.error("[copy-url-as-markdown] Failed to copy to clipboard:", e);
      });
    },
  },
  // ---- Split View Actions ----
  {
    name: "floorp-split-view-open-left",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-open-left: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            selectedTab: unknown;
            addTabSplitView: (tabs: unknown[]) => unknown;
            addTrustedTab: (url: string) => unknown;
            activeSplitView: {
              tabs: unknown[];
              addTabs: (tabs: unknown[]) => void;
            } | null;
            moveTabBefore: (tab: unknown, beforeTab: unknown) => void;
            removeTab: (tab: unknown) => void;
          };
        }
      ).gBrowser;
      if (!gBrowser?.addTabSplitView) {
        return;
      }
      const activeSplitView = gBrowser.activeSplitView;
      if (activeSplitView) {
        if (activeSplitView.tabs.length >= 4) {
          console.debug(
            "[mouse-gesture:split-view]",
            "floorp-split-view-open-left: max panes reached",
          );
          return;
        }
        const existingCount = activeSplitView.tabs.length;
        const newTab = gBrowser.addTrustedTab("about:opentabs");
        try {
          activeSplitView.addTabs([newTab]);
        } catch (e) {
          console.error("[MouseGestures] Failed to add tab to split view:", e);
          gBrowser.removeTab(newTab);
          throw e;
        }
        const firstTab = activeSplitView.tabs[0];
        if (firstTab && firstTab !== newTab) {
          gBrowser.moveTabBefore(newTab, firstTab);
        }
        if (existingCount === 2) {
          applyThreePaneLayout(win, "grid-3pane-left-main");
        }
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-open-left: added pane to existing split view",
        );
        return;
      }
      const selectedTab = gBrowser.selectedTab;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      try {
        gBrowser.addTabSplitView([newTab, selectedTab]);
      } catch (e) {
        console.error("[MouseGestures] Failed to create split view:", e);
        gBrowser.removeTab(newTab);
        throw e;
      }
    },
  },
  {
    name: "floorp-split-view-open-right",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-open-right: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            selectedTab: unknown;
            addTabSplitView: (tabs: unknown[]) => unknown;
            addTrustedTab: (url: string) => unknown;
            activeSplitView: {
              tabs: unknown[];
              addTabs: (tabs: unknown[]) => void;
            } | null;
            moveTabBefore: (tab: unknown, beforeTab: unknown) => void;
            removeTab: (tab: unknown) => void;
          };
        }
      ).gBrowser;
      if (!gBrowser?.addTabSplitView) {
        return;
      }
      const activeSplitView = gBrowser.activeSplitView;
      if (activeSplitView) {
        if (activeSplitView.tabs.length >= 4) {
          console.debug(
            "[mouse-gesture:split-view]",
            "floorp-split-view-open-right: max panes reached",
          );
          return;
        }
        const existingCount = activeSplitView.tabs.length;
        const newTab = gBrowser.addTrustedTab("about:opentabs");
        try {
          activeSplitView.addTabs([newTab]);
        } catch (e) {
          console.error("[MouseGestures] Failed to add tab to split view:", e);
          gBrowser.removeTab(newTab);
          throw e;
        }
        if (existingCount === 2) {
          applyThreePaneLayout(win, "grid-3pane-right-main");
        }
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-open-right: added pane to existing split view",
        );
        return;
      }
      const selectedTab = gBrowser.selectedTab;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      try {
        gBrowser.addTabSplitView([selectedTab, newTab]);
      } catch (e) {
        console.error("[MouseGestures] Failed to create split view:", e);
        gBrowser.removeTab(newTab);
        throw e;
      }
    },
  },
  {
    name: "floorp-split-view-open-top",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-open-top: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            selectedTab: unknown;
            addTabSplitView: (tabs: unknown[]) => unknown;
            addTrustedTab: (url: string) => unknown;
            activeSplitView: {
              tabs: unknown[];
              addTabs: (tabs: unknown[]) => void;
            } | null;
            tabpanels: { splitViewPanels: unknown[] } | null;
            moveTabBefore: (tab: unknown, beforeTab: unknown) => void;
            removeTab: (tab: unknown) => void;
          };
        }
      ).gBrowser;
      if (!gBrowser?.addTabSplitView) {
        return;
      }
      const activeSplitView = gBrowser.activeSplitView;
      if (activeSplitView) {
        if (activeSplitView.tabs.length >= 4) {
          console.debug(
            "[mouse-gesture:split-view]",
            "floorp-split-view-open-top: max panes reached",
          );
          return;
        }
        const existingCount = activeSplitView.tabs.length;
        const newTab = gBrowser.addTrustedTab("about:opentabs");
        try {
          activeSplitView.addTabs([newTab]);
        } catch (e) {
          console.error("[MouseGestures] Failed to add tab to split view:", e);
          gBrowser.removeTab(newTab);
          throw e;
        }
        const firstTab = activeSplitView.tabs[0];
        if (firstTab && firstTab !== newTab) {
          gBrowser.moveTabBefore(newTab, firstTab);
        }
        if (existingCount === 2) {
          applyThreePaneLayout(win, "grid-3pane-top-main");
        } else {
          const groupId = getSplitViewGroupIdForTabs(
            activeSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
          );
          if (groupId) {
            setPersistedGroupLayout(groupId, "vertical");
          }
        }
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-open-top: added pane to existing split view",
        );
        return;
      }
      const selectedTab = gBrowser.selectedTab;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      try {
        gBrowser.addTabSplitView([newTab, selectedTab]);
      } catch (e) {
        console.error("[MouseGestures] Failed to create split view:", e);
        gBrowser.removeTab(newTab);
        throw e;
      }
      const newActiveSplitView = gBrowser.activeSplitView;
      if (newActiveSplitView?.tabs) {
        const groupId = getSplitViewGroupIdForTabs(
          newActiveSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
        );
        if (groupId) {
          setPersistedGroupLayout(groupId, "vertical");
        }
      }
      const logger = console.createInstance({ prefix: "[mouse-gesture:split-view]" });
      applyLayoutAttribute(logger, "vertical", 2);
      const panels = gBrowser.tabpanels?.splitViewPanels as string[] | undefined;
      if (panels) {
        updateHandles(panels, "vertical");
      }
    },
  },
  {
    name: "floorp-split-view-open-bottom",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-open-bottom: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            selectedTab: unknown;
            addTabSplitView: (tabs: unknown[]) => unknown;
            addTrustedTab: (url: string) => unknown;
            activeSplitView: {
              tabs: unknown[];
              addTabs: (tabs: unknown[]) => void;
            } | null;
            tabpanels: { splitViewPanels: unknown[] } | null;
            moveTabBefore: (tab: unknown, beforeTab: unknown) => void;
            removeTab: (tab: unknown) => void;
          };
        }
      ).gBrowser;
      if (!gBrowser?.addTabSplitView) {
        return;
      }
      const activeSplitView = gBrowser.activeSplitView;
      if (activeSplitView) {
        if (activeSplitView.tabs.length >= 4) {
          console.debug(
            "[mouse-gesture:split-view]",
            "floorp-split-view-open-bottom: max panes reached",
          );
          return;
        }
        const existingCount = activeSplitView.tabs.length;
        const newTab = gBrowser.addTrustedTab("about:opentabs");
        try {
          activeSplitView.addTabs([newTab]);
        } catch (e) {
          console.error("[MouseGestures] Failed to add tab to split view:", e);
          gBrowser.removeTab(newTab);
          throw e;
        }
        if (existingCount === 2) {
          applyThreePaneLayout(win, "grid-3pane-bottom-main");
        } else {
          const groupId = getSplitViewGroupIdForTabs(
            activeSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
          );
          if (groupId) {
            setPersistedGroupLayout(groupId, "vertical");
          }
        }
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-open-bottom: added pane to existing split view",
        );
        return;
      }
      const selectedTab = gBrowser.selectedTab;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      try {
        gBrowser.addTabSplitView([selectedTab, newTab]);
      } catch (e) {
        console.error("[MouseGestures] Failed to create split view:", e);
        gBrowser.removeTab(newTab);
        throw e;
      }
      const newActiveSplitView = gBrowser.activeSplitView;
      if (newActiveSplitView?.tabs) {
        const groupId = getSplitViewGroupIdForTabs(
          newActiveSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
        );
        if (groupId) {
          setPersistedGroupLayout(groupId, "vertical");
        }
      }
      const logger = console.createInstance({ prefix: "[mouse-gesture:split-view]" });
      applyLayoutAttribute(logger, "vertical", 2);
      const panels = gBrowser.tabpanels?.splitViewPanels as string[] | undefined;
      if (panels) {
        updateHandles(panels, "vertical");
      }
    },
  },
  {
    name: "floorp-split-view-close",
    fn: (win) => {
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            activeSplitView: {
              unsplitTabs: (trigger: string | null) => void;
            } | null;
          };
        }
      ).gBrowser;
      if (!gBrowser?.activeSplitView) {
        return;
      }
      gBrowser.activeSplitView.unsplitTabs(null);
    },
  },
  {
    name: "floorp-split-view-swap-panes",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-swap-panes: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            activeSplitView: {
              tabs: unknown[];
              reverseTabs: () => void;
            } | null;
          };
        }
      ).gBrowser;
      const activeSplitView = gBrowser?.activeSplitView;
      if (!activeSplitView) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-swap-panes: no active split view",
        );
        return;
      }
      if (activeSplitView.tabs.length < 2) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-swap-panes: tabs.length < 2",
          {
            tabCount: activeSplitView.tabs.length,
          },
        );
        return;
      }
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-swap-panes: calling reverseTabs",
        {
          tabCount: activeSplitView.tabs.length,
        },
      );
      activeSplitView.reverseTabs();
    },
  },
  {
    name: "floorp-split-view-cycle-layout",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-cycle-layout: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            activeSplitView: { tabs: unknown[] } | null;
            tabpanels: { splitViewPanels: unknown[] } | null;
          };
        }
      ).gBrowser;
      const activeSplitView = gBrowser?.activeSplitView;
      if (!activeSplitView) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-cycle-layout: no active split view",
        );
        return;
      }
      if (activeSplitView.tabs.length < 2) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-cycle-layout: tabs.length < 2",
          {
            tabCount: activeSplitView.tabs.length,
          },
        );
        return;
      }

      const paneCount = activeSplitView.tabs.length;
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-cycle-layout: pane count",
        { paneCount },
      );

      const cycleMap: Record<number, SplitViewLayout[]> = {
        2: ["horizontal", "vertical"],
        3: [
          "horizontal",
          "vertical",
          "grid-3pane-left-main",
          "grid-3pane-right-main",
          "grid-3pane-top-main",
          "grid-3pane-bottom-main",
        ],
        4: ["horizontal", "vertical", "grid-2x2"],
      };
      const cycle = cycleMap[paneCount] ?? ["horizontal", "vertical"];

      if (!win.document) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-cycle-layout: win.document is null",
        );
        return;
      }
      const container = win.document.getElementById("tabbrowser-tabpanels");
      if (!container) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-cycle-layout: tabbrowser-tabpanels element not found",
        );
        return;
      }
      const currentLayout =
        (container.getAttribute("split-view-layout") as SplitViewLayout) ??
        "horizontal";
      const currentIdx = cycle.indexOf(currentLayout);
      const nextIdx = (currentIdx === -1
        ? (currentLayout.startsWith("grid-3pane-") ? 2 : 0)
        : (currentIdx + 1)) % cycle.length;
      const nextLayout = cycle[nextIdx]!;

      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-cycle-layout: cycling layout",
        {
          currentLayout,
          nextLayout,
          paneCount,
          availableLayouts: cycle,
        },
      );

      // Persist the new layout so it survives re-activation and session restore.
      const groupId = getSplitViewGroupIdForTabs(
        activeSplitView.tabs as import("#features-chrome/common/split-view/data/types.js").SplitViewTab[],
      );
      if (groupId) {
        setPersistedGroupLayout(groupId, nextLayout);
      }

      // Apply layout properly through the layout pipeline (handles, grid styles).
      const logger = console.createInstance({ prefix: "[mouse-gesture:split-view]" });
      applyLayoutAttribute(logger, nextLayout, paneCount);
      const panels = gBrowser.tabpanels?.splitViewPanels as string[] | undefined;
      if (panels) {
        updateHandles(panels, nextLayout);
      }
    },
  },
  {
    name: "floorp-split-view-add-pane",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-add-pane: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            activeSplitView: {
              tabs: unknown[];
              addTabs: (tabs: unknown[]) => void;
            } | null;
            addTrustedTab: (url: string) => unknown;
            removeTab: (tab: unknown) => void;
          };
        }
      ).gBrowser;
      const activeSplitView = gBrowser?.activeSplitView;
      if (!activeSplitView || !gBrowser) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-add-pane: no active split view or gBrowser",
          {
            hasActiveSplitView: activeSplitView != null,
            hasGBrowser: gBrowser != null,
          },
        );
        return;
      }
      if (activeSplitView.tabs.length >= 4) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-add-pane: already at max panes",
          {
            tabCount: activeSplitView.tabs.length,
          },
        );
        return;
      }
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-add-pane: adding new pane",
        {
          currentTabCount: activeSplitView.tabs.length,
        },
      );
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      try {
        activeSplitView.addTabs([newTab]);
      } catch (e) {
        console.error("[MouseGestures] Failed to add tab to split view:", e);
        gBrowser.removeTab(newTab);
        throw e;
      }
    },
  },
  {
    name: "floorp-split-view-remove-pane",
    fn: (win) => {
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-remove-pane: start",
      );
      const gBrowser = (
        win as unknown as {
          gBrowser: {
            activeSplitView: { tabs: unknown[] } | null;
            moveTabToSplitView: (tab: unknown, wrapper: null) => void;
          };
        }
      ).gBrowser;
      const activeSplitView = gBrowser?.activeSplitView;
      if (!activeSplitView || !gBrowser) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-remove-pane: no active split view or gBrowser",
          {
            hasActiveSplitView: activeSplitView != null,
            hasGBrowser: gBrowser != null,
          },
        );
        return;
      }
      if (activeSplitView.tabs.length <= 2) {
        console.debug(
          "[mouse-gesture:split-view]",
          "floorp-split-view-remove-pane: at minimum panes (2), cannot remove",
          {
            tabCount: activeSplitView.tabs.length,
          },
        );
        return;
      }
      const tabs = activeSplitView.tabs;
      const lastTab = tabs[tabs.length - 1];
      console.debug(
        "[mouse-gesture:split-view]",
        "floorp-split-view-remove-pane: removing last pane",
        {
          tabCount: tabs.length,
          lastTabIndex: tabs.length - 1,
        },
      );
      gBrowser.moveTabToSplitView(lastTab, null);
    },
  },
];
