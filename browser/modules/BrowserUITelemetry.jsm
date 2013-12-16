// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserUITelemetry"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
  "resource:///modules/RecentWindow.jsm");

// By default, we expect the following items to be in the appmenu,
// and can record clicks on them. Clicks for other items will be
// recorded as "appmenu:unrecognized" in UI Telemetry.
//
// About the :child entries - for the bookmarks, history and feed
// menu popups where the user might click on an id-less node, we
// record this as a click on the menupopup id with a :child suffix.
// We also do something similar for splitmenu items - a click recorded
// on appmenu_newTab:child is actually a click on the left half of the
// splitmenu, since the click target is an anonymous child of the
// splitmenu element.
const APPMENU_WHITELIST = [
  "appmenu_newTab:child",
    "appmenu_newTab_popup",
    "appmenu_newNavigator",
    "appmenu_openFile",

  "appmenu_newPrivateWindow",
  "appmenu_offlineModeRecovery",

  "appmenu-cut",
  "appmenu-copy",
  "appmenu-paste",
    "appmenu-editmenu-cut",
    "appmenu-editmenu-copy",
    "appmenu-editmenu-paste",
    "appmenu-editmenu-undo",
    "appmenu-editmenu-redo",
    "appmenu-editmenu-selectAll",
    "appmenu-editmenu-delete",

  "appmenu_find",
  "appmenu_savePage",
  "appmenu_sendLink",

  "appmenu_print:child",
    "appmenu_print_popup",
    "appmenu_printPreview",
    "appmenu_printSetup",

  "appmenu_webDeveloper:child",
    "appmenu_devToolbox",
    "appmenuitem_webconsole",
    "appmenuitem_inspector",
    "appmenuitem_jsdebugger",
    "appmenuitem_styleeditor",
    "appmenuitem_jsprofiler",
    "appmenuitem_netmonitor",
    "appmenu_devToolbar",
    "appmenu_devAppMgr",
    "appmenu_chromeDebugger",
    "appmenu_browserConsole",
    "appmenu_responsiveUI",
    "appmenu_scratchpad",
    "appmenu_pageSource",
    "appmenu_errorConsole",
    "appmenu_devtools_connect",
    "appmenu_getMoreDevtools",
    "appmenu_developer_chardet.off",
    "appmenu_developer_chardet.ja_parallel_state_machine",
    "appmenu_developer_chardet.ruprob",
    "appmenu_developer_chardet.ukprob",
    // We also automatically accept anything that starts
    // with "appmenu_developer_charset" - see
    // APPMENU_PREFIX_WHITELIST.
    "appmenu_offlineMode",

  "appmenu_chardet.off",
  "appmenu_chardet.ja_parallel_state_machine",
  "appmenu_chardet.ruprob",
  "appmenu_chardet.ukprob",
  // We also automatically accept anything that starts
  // with "appmenu_charset" - see APPMENU_PREFIX_WHITELIST.

  "appmenu_fullScreen",
  "sync-setup-appmenu",
  "sync-syncnowitem-appmenu",
  "appmenu-quit",

  "appmenu_bookmarks:child",
    "appmenu_bookmarksPopup:child",
      "appmenu_showAllBookmarks",
      "appmenu_bookmarkThisPage",
      "appmenu_subscribeToPage",
      "appmenu_subscribeToPageMenupopup:child",
      "appmenu_bookmarksToolbarPopup:child",
      "appmenu_unsortedBookmarks",

  "appmenu_history:child",
    "appmenu_historyMenupopup:child",
      "appmenu_showAllHistory",
      "appmenu_sanitizeHistory",
      "appmenu_sync-tabs",
      "appmenu_restoreLastSession",
      "appmenu_recentlyClosedTabsMenupopup:child",
      "appmenu_recentlyClosedWindowsMenupopup:child",

  "appmenu_downloads",
  "appmenu_addons",

  "appmenu_customize:child",
    "appmenu_customizeMenu:child",
      "appmenu_preferences",
      "appmenu_toolbarLayout",

  "appmenu_help:child",
    "appmenu_openHelp",
    "appmenu_gettingStarted",
    "appmenu_keyboardShortcuts",
    "appmenu_healthReport",
    "appmenu_troubleshootingInfo",
    "appmenu_feedbackPage",
    "appmenu_safeMode",
    "appmenu_about",
];

const APPMENU_PREFIX_WHITELIST = [
  'appmenu_developer_charset',
  'appmenu_charset',
];

this.BrowserUITelemetry = {
  init: function() {
    UITelemetry.addSimpleMeasureFunction("toolbars",
                                         this.getToolbarMeasures.bind(this));
    Services.obs.addObserver(this, "browser-delayed-startup-finished", false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "browser-delayed-startup-finished") {
      this._registerWindow(aSubject);
    }
  },

  _registerWindow: function(aWindow) {
    aWindow.addEventListener("unload", this);
    let document = aWindow.document;

    let appMenuButton = document.getElementById("appmenu-button");
    if (appMenuButton) {
      appMenuButton.addEventListener("mouseup", this);
    }
  },

  _unregisterWindow: function(aWindow) {
    aWindow.removeEventListener("unload", this);
    let document = aWindow.document;

    let appMenuButton = document.getElementById("appmenu-button");
    if (appMenuButton) {
      appMenuButton.removeEventListener("mouseup", this);
    }
  },

  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "unload":
        this._unregisterWindow(aEvent.currentTarget);
        break;
      case "mouseup":
        this._handleMouseUp(aEvent);
        break;
    }
  },

  _handleMouseUp: function(aEvent) {
    let itemId;

    if (aEvent.originalTarget.id == "appmenu-button") {
      itemId = aEvent.originalTarget.id;
    } else {
      // We must have clicked on a child of the appmenu
      itemId = this._getAppmenuItemId(aEvent);
    }

    UITelemetry.addEvent("appmenu:" + itemId, "mouseup");
  },

  _getAppmenuItemId: function(aEvent) {
    let candidate =
      aEvent.originalTarget.id ? aEvent.originalTarget.id
                               : getIDBasedOnFirstIDedAncestor(aEvent.originalTarget);

    // We only accept items that are in our whitelist OR start with
    // "charset."
    if (candidate &&
        (APPMENU_WHITELIST.indexOf(candidate) != -1 ||
         APPMENU_PREFIX_WHITELIST.some( s => candidate.startsWith(s) ))) {
      return candidate;
    }

    return "unrecognized";
  },

  countCustomizationEvent: function(aCustomizationEvent) {
    UITelemetry.addEvent("customize:" + aCustomizationEvent, null);
  },

  getToolbarMeasures: function() {
    // Grab the most recent non-popup, non-private browser window for us to
    // analyze the toolbars in...
    let win = RecentWindow.getMostRecentBrowserWindow({
      private: false,
      allowPopups: false
    });

    // If there are no such windows, we're out of luck. :(
    if (!win) {
      return {};
    }

    let document = win.document;
    let result = {};

    // Determine if the add-on bar is currently visible
    let addonBar = document.getElementById("addon-bar");
    result.addonBarEnabled = addonBar && !addonBar.collapsed;

    return result;
  },
};

/**
 * Returns the id of the first ancestor of aNode that has an id, with
 * ":child" appended to it. If aNode has no parent, or no ancestor has an
 * id, returns null.
 */
function getIDBasedOnFirstIDedAncestor(aNode) {
  while (!aNode.id) {
    aNode = aNode.parentNode;
    if (!aNode) {
      return null;
    }
  }

  return aNode.id + ":child";
}
