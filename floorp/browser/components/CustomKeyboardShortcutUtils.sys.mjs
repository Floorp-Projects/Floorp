/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports an object containing keyboard shortcut actions that can be triggered by the user.
 * Each action is represented by an array with three elements:
 * 1. The code that will be executed when the action is triggered.
 * 2. A string used for Fluent localization.
 * 3. The type of action, which can be used to filter actions or get all actions of a type.
 * The module also exports constants for the preference keys used to store the keyboard shortcuts and whether they are enabled.
 *
 *  @module CustomKeyboardShortcutUtils
 * 
 *  @example Importing the module
 *  import { CustomKeyboardShortcutUtils } from "resource:///modules/CustomKeyboardShortcutUtils.sys.mjs";
 *  const CustomKeyboardShortcutUtils = ChromeUtils.importESModule("resource:///modules/CustomKeyboardShortcutUtils.sys.mjs");
 * 
 *  @example Getting all actions of a type
 *  const allActionType = CustomKeyboardShortcutUtils.keyboradShortcutFunctions.getInfoFunctions.getAllActionType();
 *  console.log(allActionType);
 *  Codes output: ["tab-action", "page-action", "visible-action", "history-action", "search-action", "tools-action", "pip-action", "bookmark-action", "open-page-action", "history-action", "downloads-action", "sidebar-action", "bms-action"]
 * 
 *
 * @typedef {object} ShortcutAction
 * @property {string} code - The code that will be executed when the action is triggered.
 * @property {string} localizationKey - A string used for Fluent localization.
 * @property {string} type - The type of action, which can be used to filter actions or get all actions of a type.
 *
 * @typedef {object} KeyboardShortcutActions
 * @typedef {object} CustomKeyboardShortcutUtils
 * @property {string[]} EXPORTED_SYMBOLS - The exported symbols of the module.
 * @property {string} SHORTCUT_KEY_AND_ACTION_PREF - The preference key used to store the keyboard shortcuts and actions.
 * @property {string} SHORTCUT_KEY_AND_ACTION_ENABLED_PREF - The preference key used to store whether the keyboard shortcuts and actions are enabled.
 * @property {KeyboardShortcutActions} keyboradShortcutActions - The object containing all the keyboard shortcut actions.
 */

export const EXPORTED_SYMBOLS = ["CustomKeyboardShortcutUtils"];
export const SHORTCUT_KEY_AND_ACTION_PREF = "floorp.custom.shortcutkeysAndActions";
export const SHORTCUT_KEY_AND_ACTION_ENABLED_PREF = "floorp.custom.shortcutkeysAndActions.enabled";
export const SHORTCUT_KEY_DISABLE_FX_DEFAULT_SCKEY_PREF = "floorp.custom.shortcutkeysAndActions.remove.fx.actions";
export const SHORTCUT_KEY_CHANGED_ARRAY_PREF = "floorp.custom.shortcutkeysAndActions.changed";

export const keyboradShortcutActions = {

    /*
     * these are the actions that can be triggered by keyboard shortcuts.
     * the first element of each array is the code that will be executed.
     * the second element is use for Fluent localization.
     * 3rd is the type of action. You can use it to filter actions or get all actions of a type.
     * If you want to add a new action, you need to add it here.
    */

    // manage actions
    openNewTab: ["BrowserOpenTab()","open-new-tab", "tab-action"],
    closeTab: ["BrowserCloseTabOrWindow()", "close-tab", "tab-action"],
    openNewWindow: ["OpenBrowserWindow()", "open-new-window", "tab-action"],
    openNewPrivateWindow: ["OpenBrowserWindow({private: true})", "open-new-private-window", "tab-action"],
    closeWindow: ["BrowserTryToCloseWindow()", "close-window", "tab-action"],
    restoreLastTab: ["undoCloseTab()", "restore-last-session", "tab-action"],
    restoreLastWindow: ["undoCloseWindow()", "restore-last-window", "tab-action"],
    showNextTab: ["gBrowser.tabContainer.advanceSelectedTab(1, true)", "show-next-tab", "tab-action"],
    showPreviousTab: ["gBrowser.tabContainer.advanceSelectedTab(-1, true)", "show-previous-tab", "tab-action"],
    showAllTabsPanel: ["gTabsPanel.showAllTabsPanel()", "show-all-tabs-panel", "tab-action"],

    // Page actions
    sendWithMail: ["MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser)", "send-with-mail", "page-action"],
    savePage: ["saveBrowser(gBrowser.selectedBrowser)", "save-page", "page-action"],
    printPage: ["PrintUtils.startPrintWindow(gBrowser.selectedBrowser.browsingContext)", "print-page", "page-action"],
    muteCurrentTab: ["gBrowser.toggleMuteAudioOnMultiSelectedTabs(gBrowser.selectedTab)", "mute-current-tab", "page-action"],
    showSourceOfPage: ["BrowserViewSource(window.gBrowser.selectedBrowser)", "show-source-of-page", "page-action"],
    showPageInfo: ["BrowserPageInfo()", "show-page-info", "page-action"],

    // Visible actions
    zoomIn: ["FullZoom.enlarge()", "zoom-in", "visible-action"],
    zoomOut: ["FullZoom.reduce()", "zoom-out", "visible-action"],
    resetZoom: ["FullZoom.reset()", "reset-zoom", "visible-action"],

    // History actions
    back: ["BrowserBack()", "back", "history-action"],
    forward: ["BrowserForward()", "forward", "history-action"],
    stop: ["BrowserStop()", "stop", "history-action"],
    reload: ["BrowserReload()", "reload", "history-action"],
    forceReload: ["BrowserReloadSkipCache()", "force-reload", "history-action"],

    // search actions
    searchInThisPage: ["gLazyFindCommand('onFindCommand')", "search-in-this-page", "search-action"],
    showNextSearchResult: ["gLazyFindCommand('onFindAgainCommand', false)", "show-next-search-result", "search-action"],
    showPreviousSearchResult: ["gLazyFindCommand('onFindAgainCommand', true)", "show-previous-search-result", "search-action"],
    searchTheWeb: ["BrowserSearch.webSearch()", "search-the-web", "search-action"],

    // Tools actions
    openMigrationWizard: ["MigrationUtils.showMigrationWizard(window, { entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.FILE_MENU })", "open-migration-wizard", "tools-action"],
    quitFromApplication: ["goQuitApplication()", "quit-from-application", "tools-action"],
    enterIntoCustomizeMode: ["gCustomizeMode.enter()", "enter-into-customize-mode", "tools-action"],
    enterIntoOfflineMode: ["BrowserOffline.toggleOfflineStatus()", "enter-into-offline-mode", "tools-action"],
    openScreenCapture: ["ScreenshotsUtils.notify(window, 'shortcut')", "open-screen-capture", "tools-action"],

    // PIP actions
    showPIP: ["PictureInPicture.onCommand()", "show-pip", "pip-action"],

    // Bookmark actions
    bookmarkThisPage: ["gContextMenu.bookmarkThisPage()", "bookmark-this-page", "bookmark-action"],
    openBookmarksSidebar: ["toggleSidebar('viewBookmarksSidebar')", "open-bookmarks-sidebar", "bookmark-action"],
    openBookmarkAddTool: ["PlacesUIUtils.showBookmarkPagesDialog(PlacesCommandHook.uniqueCurrentPages)", "open-bookmark-add-tool", "bookmark-action"],
    openBookmarksManager: ["PlacesCommandHook.showPlacesOrganizer('UnfiledBookmarks')", "open-bookmarks-manager", "bookmark-action"],
    toggleBookmarkToolbar: ["BookmarkingUI.toggleBookmarksToolbar('bookmark-tools')", "toggle-bookmark-toolbar", "bookmark-action"],

    // Open Page actions
    openGeneralPreferences: ["openPreferences()", "open-general-preferences", "open-page-action"],
    openPrivacyPreferences: ["openPreferences('panePrivacy')", "open-privacy-preferences", "open-page-action"],
    openWorkspacesPreferences: ["openPreferences('paneWorkspaces')", "open-workspaces-preferences", "open-page-action"],
    openContainersPreferences: ["openPreferences('paneContainers')", "open-containers-preferences", "open-page-action"],
    openSearchPreferences: ["openPreferences('paneSearch')", "open-search-preferences", "open-page-action"],
    openSyncPreferences: ["openPreferences('paneSync')", "open-sync-preferences", "open-page-action"],
    openTaskManager: ["switchToTabHavingURI('about:processes', true)", "open-task-manager", "open-page-action"],
    openAddonsManager: ["BrowserOpenAddonsMgr()", "open-addons-manager", "open-page-action"],
    openHomePage: ["BrowserHome()", "open-home-page", "open-page-action"],

    // History actions
    forgetHistory: ["Sanitizer.showUI(window)", "forget-history", "history-action"],
    quickForgetHistory: ["PlacesUtils.history.clear(true)", "quick-forget-history", "history-action"],
    clearRecentHistory: ["BrowserTryToCloseWindow()", "clear-recent-history", "history-action"],
    restoreLastSession: ["SessionStore.restoreLastSession()", "restore-last-session", "history-action"],
    searchHistory: ["PlacesCommandHook.searchHistory()", "search-history", "history-action"],
    manageHistory: ["PlacesCommandHook.showPlacesOrganizer('History')", "manage-history", "history-action"],

    // Downloads actions
    openDownloads: ["DownloadsPanel.showDownloadsHistory()", "open-downloads", "downloads-action"],

    // Sidebar actions
    showBookmarkSidebar: ["SidebarUI.show('viewBookmarksSidebar')", "show-bookmark-sidebar", "sidebar-action"],
    showHistorySidebar: ["SidebarUI.show('viewHistorySidebar')", "show-history-sidebar", "sidebar-action"],
    showSyncedTabsSidebar: ["SidebarUI.show('viewTabsSidebar')", "show-synced-tabs-sidebar", "sidebar-action"],
    reverseSidebarPosition: ["SidebarUI.reversePosition()", "reverse-sidebar", "sidebar-action"],
    hideSidebar: ["SidebarUI.hide()", "hide-sidebar", "sidebar-action"],
    toggleSidebar: ["SidebarUI.toggle()", "toggle-sidebar", "sidebar-action"],

    // BMS actions
    toggleBMS: ["bmsController.controllFunctions.changeVisibleWenpanel()", "show-bsm", "bms-action"],
    showPanel1: ["bmsController.eventFunctions.contextMenu.showWithNumber(0)", "show-panel-1", "bms-action"],
    showPanel2: ["bmsController.eventFunctions.contextMenu.showWithNumber(1)", "show-panel-2", "bms-action"],
    showPanel3: ["bmsController.eventFunctions.contextMenu.showWithNumber(2)", "show-panel-3", "bms-action"],
    showPanel4: ["bmsController.eventFunctions.contextMenu.showWithNumber(3)", "show-panel-4", "bms-action"],
    showPanel5: ["bmsController.eventFunctions.contextMenu.showWithNumber(4)", "show-panel-5", "bms-action"],
    showPanel6: ["bmsController.eventFunctions.contextMenu.showWithNumber(5)", "show-panel-6", "bms-action"],
    showPanel7: ["bmsController.eventFunctions.contextMenu.showWithNumber(6)", "show-panel-7", "bms-action"],
    showPanel8: ["bmsController.eventFunctions.contextMenu.showWithNumber(7)", "show-panel-8", "bms-action"],
    showPanel9: ["bmsController.eventFunctions.contextMenu.showWithNumber(8)", "show-panel-9", "bms-action"],
    showPanel10: ["bmsController.eventFunctions.contextMenu.showWithNumber(9)", "show-panel-10", "bms-action"],

    // Workspace actions
    openNextWorkspace: ["workspaceFunctions.manageWorkspaceFunctions.changeWorkspaceToBeforeNext();", "open-next-workspace", "workspace-action"],
    openPreviousWorkspace: ["workspaceFunctions.manageWorkspaceFunctions.changeWorkspaceToAfterNext();", "open-previous-workspace", "workspace-action"],
}

export const modifiersList = {
    Alt: ["Alt", "VK_ALT"],
    Control: ["Control", "VK_CONTROL"],
    Shift: ["Shift", "VK_SHIFT"],
    Tab: ["Tab", "VK_TAB"],
}

export const cannotUseModifiers = ["Zenkaku", "Hankaku", "NumLock", "Delete", "Insert", "Alphanumeric", "Unidentified", "NonConvert"]

export const keyCodesList = {
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

    removeKeyboradShortcutByAllNames(actionName, key, keyCode, modifiers) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let newKeysState = keysState.filter(keyState => { return !(keyState.actionName === actionName && keyState.key === key && keyState.keyCode === keyCode && keyState.modifiers === modifiers) });
      Services.prefs.setStringPref(SHORTCUT_KEY_AND_ACTION_PREF, JSON.stringify(newKeysState));
    },
    removeKeyboradShortcutByActionName(actionName) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let newKeysState = keysState.filter(keyState => { return !(keyState.actionName === actionName) });
      Services.prefs.setStringPref(SHORTCUT_KEY_AND_ACTION_PREF, JSON.stringify(newKeysState));
    },
  },

  getInfoFunctions: {
    getAllActionType() {
      let result = [];
      for (let actionName in keyboradShortcutActions) {
        if (!result.includes(keyboradShortcutActions[actionName][2])) {
          result.push(keyboradShortcutActions[actionName][2]);
        }
      }
      return result;
    },

    getTypeLocalization(type) {
      return `floorp-custom-actions-${type}`;
    },

    getkeyboradShortcutActionsByType(type) {
      let result = [];
      for (let actionName in keyboradShortcutActions) {
        if (keyboradShortcutActions[actionName][2] === type) {
          result.push(actionName);
        }
      }
      return result;
    },

    getkeyboradShortcutActions() {
      let result = [];
      for (let actionName in keyboradShortcutActions) {
        result.push(actionName);
      }
      return result;
    },

    getAllActionLocalizations() {
      let result = [];
      for (let actionName in keyboradShortcutActions) {
        result.push(keyboradShortcutFunctions.getInfoFunctions.getFluentLocalization(actionName));
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
      let configs = keyboradShortcutFunctions.getInfoFunctions.getAllExsitKeys();
      for (let config of configs) {
        result.push(config.actionName);
      }
      return result;
    },

    actionIsExsit(actionName) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let exsitKey = keysState.find(keyState => keyState.actionName === actionName);
      if (exsitKey) {
        return true;
      }
      return false;
    },

    getActionKey(actionName) {
      let keysState = JSON.parse(Services.prefs.getStringPref(SHORTCUT_KEY_AND_ACTION_PREF));
      let exsitKey = keysState.find(keyState => keyState.actionName === actionName);
      if (exsitKey) {
        return exsitKey;
      }
      return null;
    },   

    getFluentLocalization(actionName) {
      if (!keyboradShortcutActions[actionName]) {
        console.error(`actionName(${actionName}) is not exsit`);
        return null;
      }
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
