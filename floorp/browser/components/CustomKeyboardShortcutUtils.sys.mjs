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
    openNewTab: ["BrowserOpenTab()","open-new-tab"],
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

    // Workspaces actions

    // Developer actions
    
}

export const modifiersList = {
    Alt: ["Alt", "VK_ALT"],
    Control: ["Control", "VK_CONTROL"],
    Shift: ["Shift", "VK_SHIFT"],
    Tab: ["Tab", "VK_TAB"],
}

export const keyCodesList = {
    // Arrow keys
    ArrowLeft: ["ArrowLeft", "VK_LEFT"],
    ArrowUp: ["ArrowUp", "VK_UP"],
    ArrowRight: ["ArrowRight", "VK_RIGHT"],
    ArrowDown: ["ArrowDown", "VK_DOWN"],

    // F num keys
    F1: ["F1", "VK_F1"],
    F2: ["F2", "VK_F2"],
    F3: ["F3", "VK_F3"],
    F4: ["F4", "VK_F4"],
    F5: ["F5", "VK_F5"],
    F6: ["F6", "VK_F6"],
    F7: ["F7", "VK_F7"],
    F8: ["F8", "VK_F8"],
    F9: ["F9", "VK_F9"],
    F10: ["F10", "VK_F10"],
    F11: ["F11", "VK_F11"],
    F12: ["F12", "VK_F12"],
    F13: ["F13", "VK_F13"],
    F14: ["F14", "VK_F14"],
    F15: ["F15", "VK_F15"],
    F16: ["F16", "VK_F16"],
    F17: ["F17", "VK_F17"],
    F18: ["F18", "VK_F18"],
    F19: ["F19", "VK_F19"],
    F20: ["F20", "VK_F20"],
    F21: ["F21", "VK_F21"],
    F22: ["F22", "VK_F22"],
    F23: ["F23", "VK_F23"],
    F24: ["F24", "VK_F24"],
}

export const cannotUseModifiers = ["Zenkaku", "Hankaku", "NumLock", "Delete", "Insert", "Alphanumeric", "Unidentified", "NonConvert"]

export const keyboradShortcutFunctions = {
  preferencesFunctions: {
    addKeyForShortcutAction(actionName, key, keyCode, modifiers) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let keyState = {
          actionName,
          key : key ? key : "",
          keyCode : keyCode ? keyCode : "",
          modifiers: modifiers ? modifiers : "",
      };
      keysState.push(keyState);

      if (keyboradShortcutFunctions.getInfoFunctions.getKeyForShortcutAction(actionName)) {
        return;
      }
      Services.prefs.setStringPref(SHORTCUT_KEY_AND_ACTION_PREF, JSON.stringify(keysState));
    },

    removeAllKeyboradShortcut() {
      Services.prefs.clearUserPref(SHORTCUT_KEY_AND_ACTION_PREF);
    },

    removeKeyboradShortcut(actionName, key, keyCode, modifiers) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let newKeysState = keysState.filter(keyState => { return !(keyState.actionName === actionName && keyState.key === key && keyState.keyCode === keyCode && keyState.modifiers === modifiers) });
      Services.prefs.setStringPref(SHORTCUT_KEY_AND_ACTION_PREF, JSON.stringify(newKeysState));
    }
  },

  getInfoFunctions: {
    getkeyboradShortcutActions() {
      let result = [];
      for (let actionName in keyboradShortcutActions) {
        result.push(actionName);
      }
      return result;
    },

    getKeyForShortcutAction(actionName) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let exsitKey = keysState.find(keyState => keyState.actionName === actionName);
      if (exsitKey) {
        return exsitKey;
      }
      return null;
    },
    
    getAllExsitKeys() {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      return keysState;
    },
    
    getAllExsitActionsName() {
      let result = [];
      let configs = keyboradShortcutFunctions.preferencesFunctions.getAllExsitKeys();
      for (let config of configs) {
        result.push(config.actionName);
      }
      return result;
    },

    getFluentLocalization(actionName) {
      return `floorp-custom-actions-${keyboradShortcutActions[actionName][1]}`;
    },

    getActionNameByKey(key, keyCode, modifiers) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let result = []
      for (let keyState of keysState) {
        if (keyState.key === key && keyState.keyCode === keyCode && keyState.modifiers === modifiers) {
          result.push(keyState.actionName)
        }
      }
      return result
    },
  },

  modifiersListFunctions: {
    getModifiersList() {
      let result = [];
      for (let modifier in modifiersList) {
        result.push(modifier);
      }
      return result;
    },

    // Not recommended to use
    getKeyIsModifier(key, keyCode) {
      let modifiersList = keyboradShortcutFunctions.modifiersListFunctions.getModifiersList();
      return modifiersList.includes(key) || modifiersList.includes(keyCode);
    },

    // Not recommended to use
    conversionToXULModifiers(modifiers) {
      let result = [];
      for (let modifier of modifiers) {
        result.push(modifiersList[modifier][1]);
      }
      return result;
    }
  },

  keyCodesListFunctions: {
    getKeyCodesList() {
      let result = [];
      for (let keyCode in keyCodesList) {
        result.push(keyCode);
      }
      return result;
    },

    conversionToXULKeyCode(keyCode) {
      if (!keyCodesList[keyCode]) {
        return null;
      }
      return keyCodesList[keyCode][1];
    }
  },

  openDialog(actionN) {
    let parentWindow = Services.wm.getMostRecentWindow("navigator:browser");

    let object = undefined;
    if (actionN) {
      object = { actionName: actionN };
    }

    if (
      parentWindow?.document.documentURI ==
      "chrome://browser/content/hiddenWindowMac.xhtml"
    ) {
      parentWindow = null;
    }
    if (parentWindow?.gDialogBox) {
      parentWindow.gDialogBox.open(
        "chrome://browser/content/preferences/dialogs/manage-keyboard-shortcut.xhtml",
        object
      );
    } else {
      Services.ww.openWindow(
        parentWindow,
        "chrome://browser/content/preferences/dialogs/manage-keyboard-shortcut.xhtml",
        null,
        "chrome,titlebar,dialog,centerscreen,modal",
        object
      );
    }
  },
}
