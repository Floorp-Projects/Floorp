// @ts-nocheck
// eslint-disable no-unsafe-optional-chaining
import type { GestureActionRegistration } from "./gestures.ts";

const getXulElement = (id: string): XULElement | null => {
  try {
    return (document?.getElementById(id) as XULElement | null) ?? null;
  } catch (_e) {
    return null;
  }
};

const doCommandIfExists = (id: string) => {
  const el = getXulElement(id);
  try {
    el?.doCommand?.();
  } catch (e) {
    console.error(`[mouse-gesture] Failed to run command for ${id}:`, e);
  }
};

const clickIfExists = (id: string) => {
  const el = getXulElement(id);
  try {
    el?.click?.();
  } catch (e) {
    console.error(`[mouse-gesture] Failed to click ${id}:`, e);
  }
};

export const actions: GestureActionRegistration[] = [
  {
    name: "gecko-back",
    fn: (win) =>
      (win.document?.getElementById("back-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-forward",
    fn: (win) =>
      (win.document?.getElementById("forward-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-reload",
    fn: (win) =>
      (win.document?.getElementById("reload-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-close-tab",
    fn: (win) => win.gBrowser.removeTab(win.gBrowser.selectedTab),
  },
  {
    name: "gecko-open-new-tab",
    fn: (win) =>
      (
        win.document?.getElementById("tabs-newtab-button") as XULElement
      ).doCommand(),
  },
  {
    name: "gecko-duplicate-tab",
    fn: (win) => win.gBrowser.duplicateTab(win.gBrowser.selectedTab),
  },
  {
    name: "gecko-reload-all-tabs",
    fn: (win) => win.gBrowser.reloadAllTabs(),
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
    name: "gecko-show-all-tabs-panel",
    fn: (win) => win.gTabsPanel.showAllTabsPanel(),
  },
  {
    name: "gecko-force-reload",
    fn: (win) => win.BrowserReloadSkipCache(),
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
    name: "gecko-open-home-page",
    fn: (win) =>
      win.switchToTabHavingURI(
        Services.prefs.getStringPref("browser.startup.homepage"),
        true,
      ),
  },
  {
    name: "gecko-open-addons-manager",
    fn: (win) => win.BrowserOpenAddonsMgr(),
  },
  {
    name: "gecko-restore-last-tab",
    fn: (win) => {
      try {
        if (win?.undoCloseTab) {
          win.undoCloseTab();
          return;
        }

        const undoMenuItem = win.document?.getElementById(
          "toolbar-context-undoCloseTab",
        );

        if (undoMenuItem instanceof XULElement) {
          undoMenuItem.doCommand();
        }
      } catch (error) {
        console.error("[mouse-gesture] Failed to trigger undoCloseTab:", error);
      }
    },
  },
  {
    name: "gecko-send-with-mail",
    fn: (win) =>
      win.MailIntegration.sendLinkForBrowser(
        win.gBrowser.selectedBrowser,
      ),
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
      win.gBrowser.toggleMuteAudioOnMultiSelectedTabs(
        win.gBrowser.selectedTab,
      ),
  },
  {
    name: "gecko-show-source-of-page",
    fn: (win) => win.BrowserViewSource(win.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-show-page-info",
    fn: (win) => win.BrowserPageInfo(),
  },
  {
    name: "floorp-rest-mode",
    fn: (win) => win.gFloorpCommands.enableRestMode(),
  },
  {
    name: "floorp-hide-user-interface",
    fn: (win) => win.gFloorpDesign.hideUserInterface(),
  },
  {
    name: "floorp-toggle-navigation-panel",
    fn: (win) => win.gFloorpDesign.toggleNavigationPanel(),
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
    fn: (win) =>
      (
        win.document?.getElementById("picture-in-picture-button") as XULElement
      ).click(),
  },
  {
    name: "gecko-restore-last-tab",
    fn: (win) => win.SessionStore.undoCloseTab(win, 0),
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
    fn: (win) => win.SidebarController.toggle(),
  },
  {
    name: "gecko-scroll-up",
    fn: (win) => win.goDoCommand("cmd_scrollPageUp"),
  },
  {
    name: "gecko-scroll-down",
    fn: (win) => win.goDoCommand("cmd_scrollPageDown"),
  },
  {
    name: "gecko-scroll-right",
    fn: (win) => win.goDoCommand("cmd_scrollRight"),
  },
  {
    name: "gecko-scroll-left",
    fn: (win) => win.goDoCommand("cmd_scrollLeft"),
  },
  {
    name: "gecko-scroll-to-top",
    fn: (win) => win.goDoCommand("cmd_scrollTop"),
  },
  {
    name: "gecko-scroll-to-bottom",
    fn: (win) => win.goDoCommand("cmd_scrollBottom"),
  },
  {
    name: "gecko-workspace-next",
    fn: (win) => win.workspacesFuncs.changeWorkspaceToNext(),
  },
  {
    name: "gecko-workspace-previous",
    fn: (win) => win.workspacesFuncs.changeWorkspaceToPrevious(),
  },
];
