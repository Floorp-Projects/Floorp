/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// import { CustomKeyboardShortcutUtils } from "resource:///modules/CustomKeyboardShortcutUtils.sys.mjs";
// const CustomKeyboardShortcutUtils = ChromeUtils.importESModule("resource:///modules/CustomKeyboardShortcutUtils.sys.mjs");
  

export const EXPORTED_SYMBOLS = ["CustomKeyboardShortcutUtils"];
export const SHORTCUT_KEY_AND_ACTION_PREF = "floorp.custom.shortcutkeysAndActions";
export const SHORTCUT_KEY_AND_ACTION_ENABLED_PREF = "floorp.custom.shortcutkeysAndActions.enabled";

export let keyboradShortcutActions = {

    /* these are the actions that can be triggered by keyboard shortcuts.
     * the first element of each array is the code that will be executed.
     * the second element is use for Fluent localization.
     * If you want to add a new action, you need to add it here.
    */

    // Tab actions
    openNewTab: [`gBrowser.addTab(Services.prefs.getStringPref(
      "browser.startup.homepage"
      ), {
       skipAnimation: true,
       inBackground: false,
       triggeringPrincipal:
         Services.scriptSecurityManager.getSystemPrincipal(),
     })`,
     "open-new-tab"
    ],
    closeTab: ["BrowserCloseTabOrWindow()", "close-tab"],
    openNewWindow: ["OpenBrowserWindow()", "open-new-window"],
    openNewPrivateWindow: ["OpenBrowserWindow({private: true})", "open-new-private-window"],
    closeWindow: ["BrowserTryToCloseWindow()", "close-window"],
    restoreLastTab: ["undoCloseTab()", "restore-last-session"],
    restoreLastWindow: ["undoCloseWindow()", "restore-last-window"],
    showNextTab: ["gBrowser.tabContainer.advanceSelectedTab(1, true)", "show-next-tab"],
    showPreviousTab: ["gBrowser.tabContainer.advanceSelectedTab(-1, true)", "show-previous-tab"],
    showAllTabsPanel: ["gTabsPanel.showAllTabsPanel()", "show-all-tabs-panel"],

    // Page actions
    sendWithMail: ["MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser)", "send-with-mail"],
    savePage: ["saveBrowser(gBrowser.selectedBrowser)", "save-page"],
    printPage: ["PrintUtils.startPrintWindow(gBrowser.selectedBrowser.browsingContext)", "print-page"],
    muteCurrentTab: ["gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab)", "mute-current-tab"],
    showSourceOfPage: ["BrowserViewSource(window.gBrowser.selectedBrowser)", "show-source-of-page"],
    showPageInfo: ["BrowserPageInfo()", "show-page-info"],

    // Visible actions
    zoomIn: ["FullZoom.enlarge()", "zoom-in"],
    zoomOut: ["FullZoom.reduce()", "zoom-out"],
    resetZoom: ["FullZoom.reset()", "reset-zoom"],

    // History actions
    back: ["BrowserBack()", "back"],
    forward: ["BrowserForward()", "forward"],
    stop: ["BrowserStop()", "stop"],
    reload: ["BrowserReload()", "reload"],
    forceReload: ["BrowserReloadSkipCache()", "force-reload"],

    // search actions
    searchInThisPage: ["gLazyFindCommand('onFindCommand')", "search-in-this-page"],
    showNextSearchResult: ["gLazyFindCommand('onFindAgainCommand', false)", "show-next-search-result"],
    showPreviousSearchResult: ["gLazyFindCommand('onFindAgainCommand', true)", "show-previous-search-result"],
    searchTheWeb: ["BrowserSearch.webSearch()", "search-the-web"],

    // Tools actions
    openMigrationWizard: ["MigrationUtils.showMigrationWizard(window, { entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU })", "open-migration-wizard"],
    quitFromApplication: ["goQuitApplication()", "quit-from-application"],
    enterIntoCustomizeMode: ["gCustomizeMode.enter()", "enter-into-customize-mode"],
    enterIntoOfflineMode: ["BrowserOffline.toggleOfflineStatus()", "enter-into-offline-mode"],
    openScreenCapture: ["ScreenshotsUtils.notify(window, 'shortcut')", "open-screen-capture"],

    // PIP actions
    showPIP: ["PictureInPicture.onCommand()", "show-pip"],

    // Bookmark actions
    bookmarkThisPage: ["gContextMenu.bookmarkThisPage()", "bookmark-this-page"],
    openBookmarksSidebar: ["toggleSidebar('viewBookmarksSidebar')", "open-bookmarks-sidebar"],
    openBookmarkAddTool: ["PlacesUIUtils.showBookmarkPagesDialog(PlacesCommandHook.uniqueCurrentPages)", "open-bookmark-add-tool"],
    openBookmarksManager: ["PlacesCommandHook.showPlacesOrganizer('UnfiledBookmarks')", "open-bookmarks-manager"],
    toggleBookmarkToolbar: ["BookmarkingUI.toggleBookmarksToolbar('bookmark-tools')", "toggle-bookmark-toolbar"],

    // Open Page actions
    openGeneralPreferences: ["openPreferences()", "open-general-preferences"],
    openPrivacyPreferences: ["openPreferences('panePrivacy')", "open-privacy-preferences"],
    openWorkspacesPreferences: ["openPreferences('paneWorkspaces')", "open-workspaces-preferences"],
    openContainersPreferences: ["openPreferences('paneContainers')", "open-containers-preferences"],
    openSearchPreferences: ["openPreferences('paneSearch')", "open-search-preferences"],
    openSyncPreferences: ["openPreferences('paneSync')", "open-sync-preferences"],
    openTaskManager: ["switchToTabHavingURI('about:processes', true)", "open-task-manager"],
    openAddonsManager: ["BrowserOpenAddonsMgr()", "open-addons-manager"],
    openHomePage: ["BrowserHome()", "open-home-page"],

    // History actions
    forgetHistory: ["Sanitizer.showUI(window)", "forget-history"],
    quickForgetHistory: ["PlacesUtils.history.clear(true)", "quick-forget-history"],
    clearRecentHistory: ["BrowserTryToCloseWindow()", "clear-recent-history"],
    restoreLastSession: ["SessionStore.restoreLastSession()", "restore-last-session"],
    searchHistory: ["PlacesCommandHook.searchHistory()", "search-history"],
    manageHistory: ["PlacesCommandHook.showPlacesOrganizer('History')", "manage-history"],

    // Downloads actions
    openDownloads: ["DownloadsPanel.showDownloadsHistory()", "open-downloads"],

    // Sidebar actions
    toggleBMS: ["bmsController.controllFunctions.changeVisibleWenpanel()", "show-bsm"],
    showBookmarkSidebar: ["SidebarUI.show('viewBookmarksSidebar')", "show-bookmark-sidebar"],
    showHistorySidebar: ["SidebarUI.show('viewHistorySidebar')", "show-history-sidebar"],
    showSyncedTabsSidebar: ["SidebarUI.show('viewTabsSidebar')", "show-synced-tabs-sidebar"],
    reverseSidebarPosition: ["SidebarUI.reversePosition()", "reverse-sidebar"],
    hideSidebar: ["SidebarUI.hide()", "hide-sidebar"],
    toggleSidebar: ["SidebarUI.toggle()", "toggle-sidebar"],
}

export const keyboradShortcutFunctions = {
  preferencesFunctions: {
    getKeyForShortcutAction(actionName) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let keyState = keysState.find(keyState => keyState[actionName]);
      if (!keyState) {
        return null;
      }
      return keyState[actionName];
    },
    addKeyForShortcutAction(actionName, key, modifiers) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let keyState = {
          actionName,
          key,
          modifiers: modifiers ? modifiers : "",
      };
      keysState.push(keyState);
      Services.prefs.setStringPref(SHORTCUT_KEY_AND_ACTION_PREF, JSON.stringify(keysState));
    }
  }
}
