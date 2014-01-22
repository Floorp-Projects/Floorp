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
  // It's true that toolbar-menubar is not visible
  // on OS X, but the XUL node is definitely present
  // in the document.
  "toolbar-menubar": [
    "menubar-items"
  ],
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

XPCOMUtils.defineLazyGetter(this, "DEFAULT_TOOLBARS", function() {
  return Object.keys(DEFAULT_TOOLBAR_SETS);
});

XPCOMUtils.defineLazyGetter(this, "ALL_BUILTIN_ITEMS", function() {
  // We also want to detect clicks on individual parts of the URL bar container,
  // so we special-case them here.
  const SPECIAL_CASES = [
    "back-button",
    "forward-button",
    "urlbar-stop-button",
    "urlbar-go-button",
    "urlbar-reload-button",
    "searchbar:child",
    "BMB_bookmarksPopup:child",
  ]
  return DEFAULT_ITEMS.concat(PALETTE_ITEMS)
                      .concat(SPECIAL_CASES);
});

const OTHER_MOUSEUP_MONITORED_ITEMS = [
   "appmenu-button",
   "PlacesChevron",
   "PlacesToolbarItems",
   "star-button",
   "menubar-items",
];

// Weakly maps browser windows to objects whose keys are relative
// timestamps for when some kind of session started. For example,
// when a customization session started. That way, when the window
// exits customization mode, we can determine how long the session
// lasted.
const WINDOW_DURATION_MAP = new WeakMap();

this.BrowserUITelemetry = {
  init: function() {
    UITelemetry.addSimpleMeasureFunction("toolbars",
                                         this.getToolbarMeasures.bind(this));
    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
    Services.obs.addObserver(this, "browser-delayed-startup-finished", false);
  },

  observe: function(aSubject, aTopic, aData) {
    switch(aTopic) {
      case "sessionstore-windows-restored":
        this._gatherFirstWindowMeasurements();
        break;
      case "browser-delayed-startup-finished":
        this._registerWindow(aSubject);
        break;
    }
  },

  /**
   * For the _countableEvents object, constructs a chain of
   * Javascript Objects with the keys in aKeys, with the final
   * key getting the value in aEndWith. If the final key already
   * exists in the final object, its value is not set. In either
   * case, a reference to the second last object in the chain is
   * returned.
   *
   * Example - suppose I want to store:
   * _countableEvents: {
   *   a: {
   *     b: {
   *       c: 0
   *     }
   *   }
   * }
   *
   * And then increment the "c" value by 1, you could call this
   * function like this:
   *
   * let example = this._ensureObjectChain([a, b, c], 0);
   * example["c"]++;
   *
   * Subsequent repetitions of these last two lines would
   * simply result in the c value being incremented again
   * and again.
   *
   * @param aKeys the Array of keys to chain Objects together with.
   * @param aEndWith the value to assign to the last key.
   * @returns a reference to the second last object in the chain -
   *          so in our example, that'd be "b".
   */
  _ensureObjectChain: function(aKeys, aEndWith) {
    let current = this._countableEvents;
    let parent = null;
    for (let [i, key] of Iterator(aKeys)) {
      if (!(key in current)) {
        if (i == aKeys.length - 1) {
          current[key] = aEndWith;
        } else {
          current[key] = {};
        }
      }
      parent = current;
      current = current[key];
    }
    return parent;
  },

  _countableEvents: {},
  _countEvent: function(aCategory, aAction) {
    let countObject = this._ensureObjectChain([aCategory, aAction], 0);
    countObject[aAction]++;
  },

  _countMouseUpEvent: function(aCategory, aAction, aButton) {
    const BUTTONS = ["left", "middle", "right"];
    let buttonKey = BUTTONS[aButton];
    if (buttonKey) {
      let countObject =
        this._ensureObjectChain([aCategory, aAction, buttonKey], 0);
      countObject[buttonKey]++;
    }
  },

  _firstWindowMeasurements: null,
  _gatherFirstWindowMeasurements: function() {
    // We'll gather measurements as soon as the session has restored.
    // We do this here instead of waiting for UITelemetry to ask for
    // our measurements because at that point all browser windows have
    // probably been closed, since the vast majority of saved-session
    // pings are gathered during shutdown.
    let win = RecentWindow.getMostRecentBrowserWindow({
      private: false,
      allowPopups: false,
    });

    // If there are no such windows, we're out of luck. :(
    this._firstWindowMeasurements = win ? this._getWindowMeasurements(win)
                                        : {};
  },

  _registerWindow: function(aWindow) {
    aWindow.addEventListener("unload", this);
    let document = aWindow.document;

    let toolbars = document.querySelectorAll("toolbar[customizable=true]");
    for (let toolbar of toolbars) {
      toolbar.addEventListener("mouseup", this);
    }

    for (let itemID of OTHER_MOUSEUP_MONITORED_ITEMS) {
      let item = document.getElementById(itemID);
      if (item) {
        item.addEventListener("mouseup", this);
      }
    }

    WINDOW_DURATION_MAP.set(aWindow, {});
  },

  _unregisterWindow: function(aWindow) {
    aWindow.removeEventListener("unload", this);
    let document = aWindow.document;

    let toolbars = document.querySelectorAll("toolbar[customizable=true]");
    for (let toolbar of toolbars) {
      toolbar.removeEventListener("mouseup", this);
    }

    for (let itemID of OTHER_MOUSEUP_MONITORED_ITEMS) {
      let item = document.getElementById(itemID);
      if (item) {
        item.removeEventListener("mouseup", this);
      }
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
    let targetID = aEvent.currentTarget.id;

    switch (targetID) {
      case "appmenu-button":
        this._appmenuMouseUp(aEvent);
        break;
      case "PlacesToolbarItems":
        this._PlacesToolbarItemsMouseUp(aEvent);
        break;
      case "PlacesChevron":
        this._PlacesChevronMouseUp(aEvent);
        break;
      case "star-button":
        this._starButtonMouseUp(aEvent);
        break;
      case "menubar-items":
        this._menubarMouseUp(aEvent);
        break;
      default:
        this._checkForBuiltinItem(aEvent);
    }
  },

  _appmenuMouseUp: function(aEvent) {
    let itemId;

    if (aEvent.originalTarget.id == "appmenu-button") {
      itemId = aEvent.originalTarget.id;
    } else {
      // We must have clicked on a child of the appmenu
      itemId = this._getAppmenuItemId(aEvent);
    }

    this._countMouseUpEvent("click-appmenu", itemId, aEvent.button);
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

  _PlacesChevronMouseUp: function(aEvent) {
    let target = aEvent.originalTarget;
    let result = target.id == "PlacesChevron" ? "chevron" : "overflowed-item";
    this._countMouseUpEvent("click-bookmarks-bar", result, aEvent.button);
  },

  _PlacesToolbarItemsMouseUp: function(aEvent) {
    let target = aEvent.originalTarget;
    // If this isn't a bookmark-item, we don't care about it.
    if (!target.classList.contains("bookmark-item")) {
      return;
    }

    let result = target.hasAttribute("container") ? "container" : "item";
    this._countMouseUpEvent("click-bookmarks-bar", result, aEvent.button);
  },

  _checkForBuiltinItem: function(aEvent) {
    let item = aEvent.originalTarget;
    // Perhaps we're seeing one of the default toolbar items
    // being clicked.
    if (ALL_BUILTIN_ITEMS.indexOf(item.id) != -1) {
      // Base case - we clicked directly on one of our built-in items,
      // and we can go ahead and register that click.
      this._countMouseUpEvent("click-builtin-item", item.id, aEvent.button);
      return;
    }

    let toolbarID = aEvent.currentTarget.id;
    // If not, we need to check if one of the ancestors of the clicked
    // item is in our list of built-in items to check.
    let candidate = getIDBasedOnFirstIDedAncestor(item, toolbarID);
    if (ALL_BUILTIN_ITEMS.indexOf(candidate) != -1) {
      this._countMouseUpEvent("click-builtin-item", candidate, aEvent.button);
    }
  },

  _starButtonMouseUp: function(aEvent) {
    let starButton = aEvent.originalTarget;
    let action = starButton.hasAttribute("starred") ? "edit" : "add";
    this._countMouseUpEvent("click-star-button", action, aEvent.button);
  },

  _menubarMouseUp: function(aEvent) {
    let target = aEvent.originalTarget;
    let tag = target.localName;
    let result = (tag == "menu" || tag == "menuitem") ? tag : "other";
    this._countMouseUpEvent("click-menubar", result, aEvent.button);
  },

  countCustomizationEvent: function(aCustomizationEvent) {
    this._countEvent("customize", aCustomizationEvent);
  },

  startCustomizing: function(aWindow) {
    this.countCustomizationEvent("start");
    let durationMap = WINDOW_DURATION_MAP.get(aWindow);
    durationMap.customization = aWindow.performance.now();
  },

  _durations: {
    customization: [],
  },

  stopCustomizing: function(aWindow) {
    let durationMap = WINDOW_DURATION_MAP.get(aWindow);
    if ("customization" in durationMap) {
      let duration = aWindow.performance.now() - durationMap.customization;
      this._durations.customization.push(duration);
      delete durationMap.customization;
    }
  },

  _getWindowMeasurements: function(aWindow) {
    let document = aWindow.document;
    let result = {};

    // Determine if the window is in the maximized, normal or
    // fullscreen state.
    result.sizemode = document.documentElement.getAttribute("sizemode");

    // Determine if the add-on bar is currently visible
    let addonBar = document.getElementById("addon-bar");
    result.addonBarEnabled = addonBar && !addonBar.collapsed;

    // Determine if the Bookmarks bar is currently visible
    let bookmarksBar = document.getElementById("PersonalToolbar");
    result.bookmarksBarEnabled = bookmarksBar && !bookmarksBar.collapsed;

    // Determine if the menubar is currently visible. On OS X, the menubar
    // is never shown, despite not having the collapsed attribute set.
    let menuBar = document.getElementById("toolbar-menubar");
    result.menuBarEnabled =
      menuBar && Services.appinfo.OS != "Darwin"
              && menuBar.getAttribute("autohide") != "true";

    // Examine the default toolbars and see what default items
    // are present and missing.
    let defaultKept = [];
    let defaultMoved = [];
    let nondefaultAdded = [];
    let customToolbars = 0;
    let addonBarItems = 0;

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
        // While we're here, if we're in the add-on bar, we're interested in
        // counting how many items (built-in AND add-on provided) are in the
        // toolbar, not counting the addonbar-closebutton and status-bar. We
        // ignore springs, spacers and separators though.
        if (toolbarID == "addon-bar" &&
            DEFAULT_TOOLBAR_SETS["addon-bar"].indexOf(item) == -1 &&
            !isSpecialToolbarItem(item)) {
          addonBarItems++;
        }

        // else, it's a generated item (like springs, spacers, etc), or
        // provided by an add-on, and we won't record it.
      }
      // Finally, let's see if this is a custom toolbar.
      if (toolbar.hasAttribute("customindex")) {
        customToolbars++;
      }
    }

    // Now go through the items in the palette to see what default
    // items are in there.
    let paletteChildren = aWindow.gNavToolbox.palette.childNodes;
    let defaultRemoved = [node.id for (node of paletteChildren)
                          if (DEFAULT_ITEMS.indexOf(node.id) != -1)];

    let addonToolbars = toolbars.length - DEFAULT_TOOLBARS.length - customToolbars;

    result.defaultKept = defaultKept;
    result.defaultMoved = defaultMoved;
    result.nondefaultAdded = nondefaultAdded;
    result.defaultRemoved = defaultRemoved;
    result.customToolbars = customToolbars;
    result.addonToolbars = addonToolbars;
    result.addonBarItems = addonBarItems;

    result.smallIcons = aWindow.gNavToolbox.getAttribute("iconsize") == "small";
    result.buttonMode = aWindow.gNavToolbox.getAttribute("mode");

    // Find out how many open tabs we have in each window
    let winEnumerator = Services.wm.getEnumerator("navigator:browser");
    let visibleTabs = [];
    let hiddenTabs = [];
    while (winEnumerator.hasMoreElements()) {
      let someWin = winEnumerator.getNext();
      if (someWin.gBrowser) {
        let visibleTabsNum = someWin.gBrowser.visibleTabs.length;
        visibleTabs.push(visibleTabsNum);
        hiddenTabs.push(someWin.gBrowser.tabs.length - visibleTabsNum);
      }
    }
    result.visibleTabs = visibleTabs;
    result.hiddenTabs = hiddenTabs;

    return result;
  },

  getToolbarMeasures: function() {
    let result = this._firstWindowMeasurements || {};
    result.countableEvents = this._countableEvents;
    result.durations = this._durations;
    return result;
  },
};

/**
 * Returns the id of the first ancestor of aNode that has an id, with
 * ":child" appended to it. If aNode has no parent, or no ancestor has an
 * id, returns null.
 *
 * @param aNode the node to find the first ID'd ancestor of
 * @param aLimitID [optional] if we reach a parent with this ID, we should
 *                 stop there.
 */
function getIDBasedOnFirstIDedAncestor(aNode, aLimitID) {
  while (!aNode.id) {
    aNode = aNode.parentNode;
    if (!aNode) {
      return null;
    }
    if (aNode.id && aNode.id == aLimitID) {
      break;
    }
  }

  return aNode.id + ":child";
}

/**
 * Returns whether or not the toolbar item ID being passed in is one of
 * our "special" items (spring, spacer, separator).
 *
 * @param aItemID the item ID to check.
 */
function isSpecialToolbarItem(aItemID) {
  return aItemID == "spring" ||
         aItemID == "spacer" ||
         aItemID == "separator";
}
