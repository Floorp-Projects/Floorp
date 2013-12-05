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

const DEFAULT_TOOLBAR_SETS = {
#ifndef XP_MACOSX
  "toolbar-menubar": [
    "menubar-items"
  ],
#endif
  "nav-bar": [
    "unified-back-forward-button",
    "urlbar-container",
    "reload-button",
    "stop-button",
    "search-container",
    "webrtc-status-button",
    "bookmarks-menu-button",
    "downloads-button",
    "home-button",
    "window-controls"
  ],
  "PersonalToolbar": [
    "personal-bookmarks"
  ],
  "TabsToolbar": [
#ifndef CAN_DRAW_IN_TITLEBAR
    "appmenu-toolbar-button",
#endif
    "tabbrowser-tabs",
    "new-tab-button",
    "alltabs-button",
    "tabs-closebutton"
  ],
  "addon-bar": [
    "addonbar-closebutton",
    "status-bar"
  ],
};

const PALETTE_ITEMS = [
  "print-button",
  "history-button",
  "bookmarks-button",
  "new-window-button",
  "fullscreen-button",
  "zoom-controls",
  "feed-button",
  "cut-button",
  "copy-button",
  "paste-button",
  "sync-button",
  "navigator-throbber",
  "tabview-button",
];

XPCOMUtils.defineLazyGetter(this, "DEFAULT_ITEMS", function() {
  let result = [];
  for (let [, buttons] of Iterator(DEFAULT_TOOLBAR_SETS)) {
    result = result.concat(buttons);
  }
  return result;
});

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

  _countableEvents: {},
  _countEvent: function(aCategory, aAction) {
    if (!(aCategory in this._countableEvents)) {
      this._countableEvents[aCategory] = {};
    }

    let categoryEvents = this._countableEvents[aCategory];

    if (!(aAction in categoryEvents)) {
      categoryEvents[aAction] = 0;
    }
    categoryEvents[aAction]++;
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

    this._countEvent("click-appmenu", itemId);
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
    this._countEvent("customize", aCustomizationEvent);
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

    // Examine the default toolbars and see what default items
    // are present and missing.
    let defaultKept = [];
    let defaultMoved = [];
    let nondefaultAdded = [];

    let toolbars = document.querySelectorAll("toolbar[customizable=true]");
    for (let toolbar of toolbars) {
      let toolbarID = toolbar.id;
      let currentset = toolbar.currentSet;
      // It's possible add-ons might have wiped out the currentSet property,
      // so we'll be a little defensive here.
      currentset = (currentset && currentset.split(",")) || [];

      for (let item of currentset) {
        // Is this a default item?
        if (DEFAULT_ITEMS.indexOf(item) != -1) {
          // Ok, it's a default item - but is it in its default
          // toolbar? We use Array.isArray instead of checking for
          // toolbarID in DEFAULT_TOOLBAR_SETS because an add-on might
          // be clever and give itself the id of "toString" or something.
          if (Array.isArray(DEFAULT_TOOLBAR_SETS[toolbarID]) &&
              DEFAULT_TOOLBAR_SETS[toolbarID].indexOf(item) != -1) {
            // The item is in its default toolbar
            defaultKept.push(item);
          } else {
            defaultMoved.push(item);
          }
        } else if (PALETTE_ITEMS.indexOf(item) != -1) {
          // It's a palette item that's been moved into a toolbar
          nondefaultAdded.push(item);
        }
        // else, it's a generated item (like springs, spacers, etc), or
        // provided by an add-on, and we won't record it.
      }
    }

    // Now go through the items in the palette to see what default
    // items are in there.
    let paletteChildren = win.gNavToolbox.palette.childNodes;
    let defaultRemoved = [node.id for (node of paletteChildren)
                          if (DEFAULT_ITEMS.indexOf(node.id) != -1)];

    result.defaultKept = defaultKept;
    result.defaultMoved = defaultMoved;
    result.nondefaultAdded = nondefaultAdded;
    result.defaultRemoved = defaultRemoved;

    result.countableEvents = this._countableEvents;

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
