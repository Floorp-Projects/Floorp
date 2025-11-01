// eslint-disable no-unsafe-optional-chaining
// deno-lint-ignore-file no-window
import type { GestureActionRegistration } from "./gestures.ts";

export const actions: GestureActionRegistration[] = [
  {
    name: "gecko-back",
    fn: () =>
      (document?.getElementById("back-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-forward",
    fn: () =>
      (document?.getElementById("forward-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-reload",
    fn: () =>
      (document?.getElementById("reload-button") as XULElement).doCommand(),
  },
  {
    name: "gecko-close-tab",
    fn: () => window.gBrowser.removeTab(window.gBrowser.selectedTab),
  },
  {
    name: "gecko-open-new-tab",
    fn: () =>
      (
        document?.getElementById("tabs-newtab-button") as XULElement
      ).doCommand(),
  },
  {
    name: "gecko-duplicate-tab",
    fn: () => window.gBrowser.duplicateTab(window.gBrowser.selectedTab),
  },
  {
    name: "gecko-reload-all-tabs",
    fn: () => window.gBrowser.reloadAllTabs(),
  },
  {
    name: "gecko-restore-last-tab",
    fn: () => window.SessionWindowUI.undoCloseTab(window),
  },
  {
    name: "gecko-open-new-window",
    fn: () => window.SessionWindowUI.undoCloseWindow(0),
  },
  {
    name: "gecko-open-new-private-window",
    fn: () => window.OpenBrowserWindow({ private: true }),
  },
  {
    name: "gecko-close-window",
    fn: () => window.BrowserTryToCloseWindow(),
  },
  {
    name: "gecko-restore-last-window",
    fn: () => window.undoCloseWindow(),
  },
  {
    name: "gecko-show-next-tab",
    fn: () => window.gBrowser.tabContainer.advanceSelectedTab(1, true),
  },
  {
    name: "gecko-show-previous-tab",
    fn: () => window.gBrowser.tabContainer.advanceSelectedTab(-1, true),
  },
  {
    name: "gecko-show-all-tabs-panel",
    fn: () => window.gTabsPanel.showAllTabsPanel(),
  },
  {
    name: "gecko-force-reload",
    fn: () => window.BrowserReloadSkipCache(),
  },
  {
    name: "gecko-zoom-in",
    fn: () => window.FullZoom.enlarge(),
  },
  {
    name: "gecko-zoom-out",
    fn: () => window.FullZoom.reduce(),
  },
  {
    name: "gecko-reset-zoom",
    fn: () => window.FullZoom.reset(),
  },
  {
    name: "gecko-bookmark-this-page",
    fn: () =>
      window.BrowserPageActions.doCommandForAction(
        window.PageActions.actionForID("bookmark"),
        null,
        window,
      ),
  },
  {
    name: "gecko-open-home-page",
    fn: () => window.BrowserHome(),
  },
  {
    name: "gecko-open-addons-manager",
    fn: () => window.BrowserOpenAddonsMgr(),
  },
  {
    name: "gecko-restore-last-tab",
    fn: () => window.undoCloseTab(),
  },
  {
    name: "gecko-send-with-mail",
    fn: () =>
      window.MailIntegration.sendLinkForBrowser(
        window.gBrowser.selectedBrowser,
      ),
  },
  {
    name: "gecko-save-page",
    fn: () => window.saveBrowser(window.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-print-page",
    fn: () =>
      window.PrintUtils.startPrintWindow(
        window.gBrowser.selectedBrowser.browsingContext,
      ),
  },
  {
    name: "gecko-mute-current-tab",
    fn: () =>
      window.gBrowser.toggleMuteAudioOnMultiSelectedTabs(
        window.gBrowser.selectedTab,
      ),
  },
  {
    name: "gecko-show-source-of-page",
    fn: () => window.BrowserViewSource(window.gBrowser.selectedBrowser),
  },
  {
    name: "gecko-show-page-info",
    fn: () => window.BrowserPageInfo(),
  },
  {
    name: "floorp-rest-mode",
    fn: () => window.gFloorpCommands.enableRestMode(),
  },
  {
    name: "floorp-hide-user-interface",
    fn: () => window.gFloorpDesign.hideUserInterface(),
  },
  {
    name: "floorp-toggle-navigation-panel",
    fn: () => window.gFloorpDesign.toggleNavigationPanel(),
  },
  {
    name: "gecko-stop",
    fn: () => window.BrowserStop(),
  },
  {
    name: "gecko-search-in-this-page",
    fn: () => window.gLazyFindCommand("onFindCommand"),
  },
  {
    name: "gecko-show-next-search-result",
    fn: () => window.gLazyFindCommand("onFindAgainCommand", false),
  },
  {
    name: "gecko-show-previous-search-result",
    fn: () => window.gLazyFindCommand("onFindAgainCommand", true),
  },
  {
    name: "gecko-search-the-web",
    fn: () => window.BrowserSearch.webSearch(),
  },
  {
    name: "gecko-open-migration-wizard",
    fn: () =>
      window.MigrationUtils.showMigrationWizard(window, {
        entrypoint: window.MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
      }),
  },
  {
    name: "gecko-quit-from-application",
    fn: () => window.Services.startup.quit(window.Ci.nsIAppStartup.eForceQuit),
  },
  {
    name: "gecko-enter-into-customize-mode",
    fn: () => window.gCustomizeMode.enter(),
  },
  {
    name: "gecko-enter-into-offline-mode",
    fn: () => window.BrowserOffline.toggleOfflineStatus(),
  },
  {
    name: "gecko-open-screen-capture",
    fn: () => window.ScreenshotsUtils.start(gBrowser.selectedBrowser),
  },
  {
    name: "floorp-show-pip",
    fn: () =>
      (
        document?.getElementById("picture-in-picture-button") as XULElement
      ).click(),
  },
  {
    name: "gecko-open-bookmark-add-tool",
    fn: () => window.PlacesCommandHook.bookmarkPage(),
  },
  {
    name: "gecko-open-bookmarks-manager",
    fn: () => window.SidebarController.toggle("viewBookmarksSidebar"),
  },
  {
    name: "gecko-toggle-bookmark-toolbar",
    fn: () => window.BookmarkingUI.toggleBookmarksToolbar("bookmark-tools"),
  },
  {
    name: "gecko-open-general-preferences",
    fn: () => window.openPreferences(),
  },
  {
    name: "gecko-open-privacy-preferences",
    fn: () => window.openPreferences("panePrivacy"),
  },
  {
    name: "gecko-open-containers-preferences",
    fn: () => window.openPreferences("paneContainers"),
  },
  {
    name: "gecko-open-search-preferences",
    fn: () => window.openPreferences("paneSearch"),
  },
  {
    name: "gecko-open-sync-preferences",
    fn: () => window.openPreferences("paneSync"),
  },
  {
    name: "gecko-open-task-manager",
    fn: () => window.switchToTabHavingURI("about:processes", true),
  },
  {
    name: "gecko-forget-history",
    fn: () => window.Sanitizer.showUI(window),
  },
  {
    name: "gecko-quick-forget-history",
    fn: () => window.PlacesUtils.history.clear(true),
  },
  {
    name: "gecko-restore-last-session",
    fn: () => window.SessionStore.restoreLastSession(),
  },
  {
    name: "gecko-search-history",
    fn: () => window.PlacesCommandHook.searchHistory(),
  },
  {
    name: "gecko-manage-history",
    fn: () => window.PlacesCommandHook.showPlacesOrganizer("History"),
  },
  {
    name: "gecko-open-downloads",
    fn: () => window.DownloadsPanel.showDownloadsHistory(),
  },
  {
    name: "gecko-show-bookmark-sidebar",
    fn: () => window.SidebarController.show("viewBookmarksSidebar"),
  },
  {
    name: "gecko-show-history-sidebar",
    fn: () => window.SidebarController.show("viewHistorySidebar"),
  },
  {
    name: "gecko-show-synced-tabs-sidebar",
    fn: () => window.SidebarController.show("viewTabsSidebar"),
  },
  {
    name: "gecko-reverse-sidebar",
    fn: () => window.SidebarController.reversePosition(),
  },
  {
    name: "gecko-hide-sidebar",
    fn: () => window.SidebarController.hide(),
  },
  {
    name: "gecko-toggle-sidebar",
    fn: () => window.SidebarController.toggle(),
  },
  {
    name: "gecko-scroll-up",
    fn: () => window.goDoCommand("cmd_scrollPageUp"),
  },
  {
    name: "gecko-scroll-down",
    fn: () => window.goDoCommand("cmd_scrollPageDown"),
  },
  {
    name: "gecko-scroll-right",
    fn: () => window.goDoCommand("cmd_scrollRight"),
  },
  {
    name: "gecko-scroll-left",
    fn: () => window.goDoCommand("cmd_scrollLeft"),
  },
  {
    name: "gecko-scroll-to-top",
    fn: () => window.goDoCommand("cmd_scrollTop"),
  },
  {
    name: "gecko-scroll-to-bottom",
    fn: () => window.goDoCommand("cmd_scrollBottom"),
  },
  {
    name: "gecko-workspace-next",
    fn: () => window.workspacesFuncs.changeWorkspaceToNext(),
  },
  {
    name: "gecko-workspace-previous",
    fn: () => window.workspacesFuncs.changeWorkspaceToPrevious(),
  },
];
