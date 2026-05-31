// SPDX-License-Identifier: MPL-2.0
// @ts-nocheck Firefox chrome globals (BrowserCommands, gBrowser, etc.) are untyped

import * as t from 'io-ts';

export const csk_category = [
  "tab-action",
  "history-action",
  "page-action",
  "visible-action",
  "search-action",
  "tools-action",
  "pip-action",
  "bookmark-action",
  "open-page-action",
  "sidebar-action",
  "workspaces-action",
  "bms-action",
  "custom-action",
  "downloads-action",
  "split-view-action",
] as const;


const CskCategoryCodec = t.keyof(
  csk_category.reduce((acc, k) => {
    acc[k] = null;
    return acc;
  }, {} as Record<typeof csk_category[number], null>)
);

const CommandsCodec = t.record(
  t.refinement(t.string, (s) => s.startsWith('gecko-') || s.startsWith('floorp-')),
  t.type({
    command: t.refinement(t.unknown, (u): u is ((ev:Event) => void) => typeof u === 'function'),
    type: CskCategoryCodec,
  })
);

type Commands = t.TypeOf<typeof CommandsCodec>;

export const commands: Commands = {
  "gecko-open-new-tab": {
    command: () => globalThis.BrowserCommands.openTab(),
    type: "tab-action",
  },
  "gecko-close-tab": {
    command: () => globalThis.BrowserCloseTabOrWindow(),
    type: "tab-action",
  },
  "gecko-open-new-window": {
    command: () => globalThis.OpenBrowserWindow(),
    type: "tab-action",
  },
  "gecko-open-new-private-window": {
    command: () => globalThis.OpenBrowserWindow({ private: true }),
    type: "tab-action",
  },
  "gecko-close-window": {
    command: () => globalThis.BrowserTryToCloseWindow(),
    type: "tab-action",
  },
  "gecko-restore-last-session": {
    command: () => globalThis.SessionStore.restoreLastSession(),
    type: "history-action",
  },
  "gecko-restore-last-window": {
    command: () => globalThis.undoCloseWindow(),
    type: "tab-action",
  },
  "gecko-show-next-tab": {
    command: () => globalThis.gBrowser.tabContainer.advanceSelectedTab(1, true),
    type: "tab-action",
  },
  "gecko-show-previous-tab": {
    command: () => globalThis.gBrowser.tabContainer.advanceSelectedTab(-1, true),
    type: "tab-action",
  },
  "gecko-show-previously-selected-tab": {
    command: () => {
      const currentTab = globalThis.gBrowser.selectedTab;
      let latest = null;
      for (const tab of globalThis.gBrowser.tabs) {
        if (tab._lastAccessed === Infinity) continue;
        // Skip currently selected tab
        if (tab === currentTab) continue;
        if (!latest || tab._lastAccessed > latest._lastAccessed) latest = tab;
      }
      if (latest) {
        globalThis.gBrowser.tabContainer._selectNewTab(latest);
      }
    },
    type: "tab-action",
  },
  "gecko-show-all-tabs-panel": {
    command: () => globalThis.gTabsPanel.showAllTabsPanel(),
    type: "tab-action",
  },
  "gecko-send-with-mail": {
    command: () =>
      globalThis.MailIntegration.sendLinkForBrowser(
        globalThis.gBrowser.selectedBrowser,
      ),
    type: "page-action",
  },
  "gecko-save-page": {
    command: () => globalThis.saveBrowser(globalThis.gBrowser.selectedBrowser),
    type: "page-action",
  },
  "gecko-print-page": {
    command: () =>
      globalThis.PrintUtils.startPrintWindow(
        globalThis.gBrowser.selectedBrowser.browsingContext,
      ),
    type: "page-action",
  },
  "gecko-mute-current-tab": {
    command: () =>
      globalThis.gBrowser.toggleMuteAudioOnMultiSelectedTabs(
        globalThis.gBrowser.selectedTab,
      ),
    type: "page-action",
  },
  "gecko-show-source-of-page": {
    command: () => globalThis.BrowserViewSource(globalThis.gBrowser.selectedBrowser),
    type: "page-action",
  },
  "gecko-show-page-info": {
    command: () => globalThis.BrowserPageInfo(),
    type: "page-action",
  },
  "floorp-rest-mode": {
    command: () => globalThis.gFloorpCommands.enableRestMode(),
    type: "page-action",
  },
  "gecko-zoom-in": {
    command: () => globalThis.FullZoom.enlarge(),
    type: "visible-action",
  },
  "gecko-zoom-out": {
    command: () => globalThis.FullZoom.reduce(),
    type: "visible-action",
  },
  "gecko-reset-zoom": {
    command: () => globalThis.FullZoom.reset(),
    type: "visible-action",
  },
  "floorp-hide-user-interface": {
    command: () => globalThis.gFloorpDesign.hideUserInterface(),
    type: "visible-action",
  },
  "gecko-back": { command: () => globalThis.BrowserBack(), type: "history-action" },
  "gecko-forward": {
    command: () => globalThis.BrowserForward(),
    type: "history-action",
  },
  "gecko-stop": { command: () => globalThis.BrowserStop(), type: "history-action" },
  "gecko-reload": {
    command: () => globalThis.BrowserReload(),
    type: "history-action",
  },
  "gecko-force-reload": {
    command: () => globalThis.BrowserReloadSkipCache(),
    type: "history-action",
  },
  "gecko-search-in-this-page": {
    command: () => globalThis.gLazyFindCommand("onFindCommand"),
    type: "search-action",
  },
  "gecko-show-next-search-result": {
    command: () => globalThis.gLazyFindCommand("onFindAgainCommand", false),
    type: "search-action",
  },
  "gecko-show-previous-search-result": {
    command: () => globalThis.gLazyFindCommand("onFindAgainCommand", true),
    type: "search-action",
  },
  "gecko-search-the-web": {
    command: () => globalThis.BrowserSearch.webSearch(),
    type: "search-action",
  },
  "gecko-open-migration-wizard": {
    command: () =>
      globalThis.MigrationUtils.showMigrationWizard(window, {
        entrypoint: globalThis.MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU,
      }),
    type: "tools-action",
  },
  "gecko-quit-from-application": {
    command: () => globalThis.Services.startup.quit(Ci.nsIAppStartup.eForceQuit),
    type: "tools-action",
  },
  "gecko-enter-into-customize-mode": {
    command: () => globalThis.gCustomizeMode.enter(),
    type: "tools-action",
  },
  "gecko-enter-into-offline-mode": {
    command: () => globalThis.BrowserOffline.toggleOfflineStatus(),
    type: "tools-action",
  },
  "gecko-gecko-open-screen-capture": {
    command: () => globalThis.ScreenshotsUtils.notify(window, "shortcut"),
    type: "tools-action",
  },
  "floorp-show-pip": {
    command: (event: Event) =>
      globalThis.gFloorpCSKActionFunctions.PictureInPicture.togglePictureInPicture(
        event,
      ),
    type: "pip-action",
  },
  "gecko-bookmark-this-page": {
    command: (event: Event) =>
      globalThis.BrowserPageActions.doCommandForAction(
        globalThis.PageActions.actionForID("bookmark"),
        event,
        this,
      ),
    type: "bookmark-action",
  },
  "gecko-open-bookmark-add-tool": {
    command: () =>
      globalThis.PlacesUIUtils.showBookmarkPagesDialog(
        globalThis.PlacesCommandHook.uniqueCurrentPages,
      ),
    type: "bookmark-action",
  },
  "gecko-open-bookmarks-manager": {
    command: () => globalThis.SidebarController.toggle("viewBookmarksSidebar"),
    type: "bookmark-action",
  },
  "gecko-toggle-bookmark-toolbar": {
    command: () =>
      globalThis.BookmarkingUI.toggleBookmarksToolbar("bookmark-tools"),
    type: "bookmark-action",
  },
  "gecko-open-general-preferences": {
    command: () => globalThis.openPreferences(),
    type: "open-page-action",
  },
  "gecko-open-privacy-preferences": {
    command: () => globalThis.openPreferences("panePrivacy"),
    type: "open-page-action",
  },
  "gecko-open-workspaces-preferences": {
    command: () => globalThis.openPreferences("paneWorkspaces"),
    type: "open-page-action",
  },
  "gecko-open-containers-preferences": {
    command: () => globalThis.openPreferences("paneContainers"),
    type: "open-page-action",
  },
  "gecko-open-search-preferences": {
    command: () => globalThis.openPreferences("paneSearch"),
    type: "open-page-action",
  },
  "gecko-open-sync-preferences": {
    command: () => globalThis.openPreferences("paneSync"),
    type: "open-page-action",
  },
  "gecko-open-task-manager": {
    command: () => globalThis.switchToTabHavingURI("about:processes", true),
    type: "open-page-action",
  },
  "gecko-open-addons-manager": {
    command: () => globalThis.BrowserOpenAddonsMgr(),
    type: "open-page-action",
  },
  "gecko-open-home-page": {
    command: () => globalThis.BrowserHome(),
    type: "open-page-action",
  },
  "gecko-forget-history": {
    command: () => globalThis.Sanitizer.showUI(window),
    type: "history-action",
  },
  "gecko-quick-forget-history": {
    command: () => globalThis.PlacesUtils.history.clear(true),
    type: "history-action",
  },
  "gecko-clear-recent-history": {
    command: () => globalThis.BrowserTryToCloseWindow(),
    type: "history-action",
  },
  "gecko-search-history": {
    command: () => globalThis.PlacesCommandHook.searchHistory(),
    type: "history-action",
  },
  "gecko-manage-history": {
    command: () => globalThis.PlacesCommandHook.showPlacesOrganizer("History"),
    type: "history-action",
  },
  "gecko-open-downloads": {
    command: () => globalThis.DownloadsPanel.showDownloadsHistory(),
    type: "downloads-action",
  },
  "gecko-show-bookmark-sidebar": {
    command: () => globalThis.SidebarController.show("viewBookmarksSidebar"),
    type: "sidebar-action",
  },
  "gecko-show-history-sidebar": {
    command: () => globalThis.SidebarController.show("viewHistorySidebar"),
    type: "sidebar-action",
  },
  "gecko-show-synced-tabs-sidebar": {
    command: () => globalThis.SidebarController.show("viewTabsSidebar"),
    type: "sidebar-action",
  },
  "gecko-reverse-sidebar": {
    command: () => globalThis.SidebarController.reversePosition(),
    type: "sidebar-action",
  },
  "gecko-hide-sidebar": {
    command: () => globalThis.SidebarController.hide(),
    type: "sidebar-action",
  },
  "gecko-toggle-sidebar": {
    command: () => globalThis.SidebarController.toggle(),
    type: "sidebar-action",
  },
  "floorp-open-previous-workspace": {
    command: () => globalThis.gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace(),
    type: "workspaces-action",
  },
  "floorp-open-next-workspace": {
    command: () =>
      globalThis.gWorkspaces.changeWorkspaceToNextOrBeforeWorkspace(true),
    type: "workspaces-action",
  },
  "floorp-show-bsm": {
    command: () =>
      globalThis.gBrowserManagerSidebar.controllFunctions.toggleBMSShortcut(),
    type: "bms-action",
  },
  "floorp-show-panel-1": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(0),
    type: "bms-action",
  },
  "floorp-show-panel-2": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(1),
    type: "bms-action",
  },
  "floorp-show-panel-3": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(2),
    type: "bms-action",
  },
  "floorp-show-panel-4": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(3),
    type: "bms-action",
  },
  "floorp-show-panel-5": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(4),
    type: "bms-action",
  },
  "floorp-show-panel-6": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(5),
    type: "bms-action",
  },
  "floorp-show-panel-7": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(6),
    type: "bms-action",
  },
  "floorp-show-panel-8": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(7),
    type: "bms-action",
  },
  "floorp-show-panel-9": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(8),
    type: "bms-action",
  },
  "floorp-show-panel-10": {
    command: () => globalThis.gBrowserManagerSidebar.contextMenu.showWithNumber(9),
    type: "bms-action",
  },
  "floorp-open-split-view-on-left": {
    command: () => {
      const gBrowser = globalThis.gBrowser;
      if (!gBrowser?.addTabSplitView || gBrowser.activeSplitView) return;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      gBrowser.addTabSplitView([newTab, gBrowser.selectedTab]);
    },
    type: "split-view-action",
  },
  "floorp-open-split-view-on-right": {
    command: () => {
      const gBrowser = globalThis.gBrowser;
      if (!gBrowser?.addTabSplitView || gBrowser.activeSplitView) return;
      const newTab = gBrowser.addTrustedTab("about:opentabs");
      gBrowser.addTabSplitView([gBrowser.selectedTab, newTab]);
    },
    type: "split-view-action",
  },
  "floorp-close-split-view": {
    command: () => {
      const gBrowser = globalThis.gBrowser;
      if (!gBrowser?.activeSplitView) return;
      gBrowser.activeSplitView.unsplitTabs(null);
    },
    type: "split-view-action",
  },
  "floorp-custom-action-1": {
    command: () =>
      globalThis.gFloorpCustomActionFunctions.evalCustomeActionWithNum(1),
    type: "custom-action",
  },
  "floorp-custom-action-2": {
    command: () =>
      globalThis.gFloorpCustomActionFunctions.evalCustomeActionWithNum(2),
    type: "custom-action",
  },
  "floorp-custom-action-3": {
    command: () =>
      globalThis.gFloorpCustomActionFunctions.evalCustomeActionWithNum(3),
    type: "custom-action",
  },
  "floorp-custom-action-4": {
    command: () =>
      globalThis.gFloorpCustomActionFunctions.evalCustomeActionWithNum(4),
    type: "custom-action",
  },
  "floorp-custom-action-5": {
    command: () =>
      globalThis.gFloorpCustomActionFunctions.evalCustomeActionWithNum(5),
    type: "custom-action",
  },
};
