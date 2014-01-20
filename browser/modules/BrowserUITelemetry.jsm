// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserUITelemetry"];

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

XPCOMUtils.defineLazyGetter(this, "DEFAULT_AREA_PLACEMENTS", function() {
  let result = {
    "PanelUI-contents": [
      "edit-controls",
      "zoom-controls",
      "new-window-button",
      "privatebrowsing-button",
      "save-page-button",
      "print-button",
      "history-panelmenu",
      "fullscreen-button",
      "find-button",
      "preferences-button",
      "add-ons-button",
      "developer-button",
    ],
    "nav-bar": [
      "urlbar-container",
      "search-container",
      "webrtc-status-button",
      "bookmarks-menu-button",
      "downloads-button",
      "home-button",
      "social-share-button",
    ],
    // It's true that toolbar-menubar is not visible
    // on OS X, but the XUL node is definitely present
    // in the document.
    "toolbar-menubar": [
      "menubar-items",
    ],
    "TabsToolbar": [
      "tabbrowser-tabs",
      "new-tab-button",
      "alltabs-button",
      "tabs-closebutton",
    ],
    "PersonalToolbar": [
      "personal-bookmarks",
    ],
  };

  let showCharacterEncoding = Services.prefs.getComplexValue(
    "browser.menu.showCharacterEncoding",
    Ci.nsIPrefLocalizedString
  ).data;
  if (showCharacterEncoding == "true") {
    result["PanelUI-contents"].push("characterencoding-button");
  }

  if (Services.sysinfo.getProperty("hasWindowsTouchInterface")) {
    result["PanelUI-contents"].push("switch-to-metro-button");
  }

  return result;
});

XPCOMUtils.defineLazyGetter(this, "DEFAULT_AREAS", function() {
  return Object.keys(DEFAULT_AREA_PLACEMENTS);
});

XPCOMUtils.defineLazyGetter(this, "PALETTE_ITEMS", function() {
  let result = [
    "open-file-button",
    "developer-button",
    "feed-button",
    "email-link-button",
    "sync-button",
    "tabview-button",
  ];

  let panelPlacements = DEFAULT_AREA_PLACEMENTS["PanelUI-contents"];
  if (panelPlacements.indexOf("characterencoding-button") == -1) {
    result.push("characterencoding-button");
  }

  return result;
});

XPCOMUtils.defineLazyGetter(this, "DEFAULT_ITEMS", function() {
  let result = [];
  for (let [, buttons] of Iterator(DEFAULT_AREA_PLACEMENTS)) {
    result = result.concat(buttons);
  }
  return result;
});

XPCOMUtils.defineLazyGetter(this, "ALL_BUILTIN_ITEMS", function() {
  // These special cases are for click events on built-in items that are
  // contained within customizable items (like the navigation widget).
  const SPECIAL_CASES = [
    "back-button",
    "forward-button",
    "urlbar-stop-button",
    "urlbar-go-button",
    "urlbar-reload-button",
    "searchbar",
    "cut-button",
    "copy-button",
    "paste-button",
    "zoom-out-button",
    "zoom-reset-button",
    "zoom-in-button",
    "BMB_bookmarksPopup",
    "BMB_unsortedBookmarksPopup",
    "BMB_bookmarksToolbarPopup",
  ]
  return DEFAULT_ITEMS.concat(PALETTE_ITEMS)
                      .concat(SPECIAL_CASES);
});

const OTHER_MOUSEUP_MONITORED_ITEMS = [
  "PlacesChevron",
  "PlacesToolbarItems",
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
    CustomizableUI.addListener(this);
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
  _countEvent: function(aKeyArray) {
    let countObject = this._ensureObjectChain(aKeyArray, 0);
    let lastItemKey = aKeyArray[aKeyArray.length - 1];
    countObject[lastItemKey]++;
  },

  _countMouseUpEvent: function(aCategory, aAction, aButton) {
    const BUTTONS = ["left", "middle", "right"];
    let buttonKey = BUTTONS[aButton];
    if (buttonKey) {
      this._countEvent([aCategory, aAction, buttonKey]);
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

    for (let areaID of CustomizableUI.areas) {
      let areaNode = document.getElementById(areaID);
      if (areaNode) {
        (areaNode.customizationTarget || areaNode).addEventListener("mouseup", this);
      }
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

    for (let areaID of CustomizableUI.areas) {
      let areaNode = document.getElementById(areaID);
      if (areaNode) {
        (areaNode.customizationTarget || areaNode).removeEventListener("mouseup", this);
      }
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
      case "PlacesToolbarItems":
        this._PlacesToolbarItemsMouseUp(aEvent);
        break;
      case "PlacesChevron":
        this._PlacesChevronMouseUp(aEvent);
        break;
      case "menubar-items":
        this._menubarMouseUp(aEvent);
        break;
      default:
        this._checkForBuiltinItem(aEvent);
    }
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

  _menubarMouseUp: function(aEvent) {
    let target = aEvent.originalTarget;
    let tag = target.localName
    let result = (tag == "menu" || tag == "menuitem") ? tag : "other";
    this._countMouseUpEvent("click-menubar", result, aEvent.button);
  },

  _bookmarksMenuButtonMouseUp: function(aEvent) {
    let bookmarksWidget = CustomizableUI.getWidget("bookmarks-menu-button");
    if (bookmarksWidget.areaType == CustomizableUI.TYPE_MENU_PANEL) {
      // In the menu panel, only the star is visible, and that opens up the
      // bookmarks subview.
      this._countMouseUpEvent("click-bookmarks-menu-button", "in-panel",
                              aEvent.button);
    } else {
      let clickedItem = aEvent.originalTarget;
      // Did we click on the star, or the dropmarker? The star
      // has an anonid of "button". If we don't find that, we'll
      // assume we clicked on the dropmarker.
      let action = "menu";
      if (clickedItem.getAttribute("anonid") == "button") {
        // We clicked on the star - now we just need to record
        // whether or not we're adding a bookmark or editing an
        // existing one.
        let bookmarksMenuNode =
          bookmarksWidget.forWindow(aEvent.target.ownerGlobal).node;
        action = bookmarksMenuNode.hasAttribute("starred") ? "edit" : "add";
      }
      this._countMouseUpEvent("click-bookmarks-menu-button", action,
                              aEvent.button);
    }
  },

  _checkForBuiltinItem: function(aEvent) {
    let item = aEvent.originalTarget;

    // We special-case the bookmarks-menu-button, since we want to
    // monitor more than just clicks on it.
    if (item.id == "bookmarks-menu-button" ||
        getIDBasedOnFirstIDedAncestor(item) == "bookmarks-menu-button") {
      this._bookmarksMenuButtonMouseUp(aEvent);
      return;
    }

    // Perhaps we're seeing one of the default toolbar items
    // being clicked.
    if (ALL_BUILTIN_ITEMS.indexOf(item.id) != -1) {
      // Base case - we clicked directly on one of our built-in items,
      // and we can go ahead and register that click.
      this._countMouseUpEvent("click-builtin-item", item.id, aEvent.button);
      return;
    }

    // If not, we need to check if one of the ancestors of the clicked
    // item is in our list of built-in items to check.
    let candidate = getIDBasedOnFirstIDedAncestor(item);
    if (ALL_BUILTIN_ITEMS.indexOf(candidate) != -1) {
      this._countMouseUpEvent("click-builtin-item", candidate, aEvent.button);
    }
  },

  _getWindowMeasurements: function(aWindow) {
    let document = aWindow.document;
    let result = {};

    // Determine if the window is in the maximized, normal or
    // fullscreen state.
    result.sizemode = document.documentElement.getAttribute("sizemode");

    // Determine if the Bookmarks bar is currently visible
    let bookmarksBar = document.getElementById("PersonalToolbar");
    result.bookmarksBarEnabled = bookmarksBar && !bookmarksBar.collapsed;

    // Determine if the menubar is currently visible. On OS X, the menubar
    // is never shown, despite not having the collapsed attribute set.
    let menuBar = document.getElementById("toolbar-menubar");
    result.menuBarEnabled =
      menuBar && Services.appinfo.OS != "Darwin"
              && menuBar.getAttribute("autohide") != "true";

    // Examine all customizable areas and see what default items
    // are present and missing.
    let defaultKept = [];
    let defaultMoved = [];
    let nondefaultAdded = [];

    for (let areaID of CustomizableUI.areas) {
      let items = CustomizableUI.getWidgetIdsInArea(areaID);
      for (let item of items) {
        // Is this a default item?
        if (DEFAULT_ITEMS.indexOf(item) != -1) {
          // Ok, it's a default item - but is it in its default
          // toolbar? We use Array.isArray instead of checking for
          // toolbarID in DEFAULT_AREA_PLACEMENTS because an add-on might
          // be clever and give itself the id of "toString" or something.
          if (Array.isArray(DEFAULT_AREA_PLACEMENTS[areaID]) &&
              DEFAULT_AREA_PLACEMENTS[areaID].indexOf(item) != -1) {
            // The item is in its default toolbar
            defaultKept.push(item);
          } else {
            defaultMoved.push(item);
          }
        } else if (PALETTE_ITEMS.indexOf(item) != -1) {
          // It's a palette item that's been moved into a toolbar
          nondefaultAdded.push(item);
        }
        // else, it's provided by an add-on, and we won't record it.
      }
    }

    // Now go through the items in the palette to see what default
    // items are in there.
    let paletteItems =
      CustomizableUI.getUnusedWidgets(aWindow.gNavToolbox.palette);
    let defaultRemoved = [item.id for (item of paletteItems)
                          if (DEFAULT_ITEMS.indexOf(item.id) != -1)];

    result.defaultKept = defaultKept;
    result.defaultMoved = defaultMoved;
    result.nondefaultAdded = nondefaultAdded;
    result.defaultRemoved = defaultRemoved;

    // Next, determine how many add-on provided toolbars exist.
    let addonToolbars = 0;
    let toolbars = document.querySelectorAll("toolbar[customizable=true]");
    for (let toolbar of toolbars) {
      if (DEFAULT_AREAS.indexOf(toolbar.id) == -1) {
        addonToolbars++;
      }
    }
    result.addonToolbars = addonToolbars;

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

  countCustomizationEvent: function(aEventType) {
    this._countEvent(["customize", aEventType]);
  },

  _durations: {
    customization: [],
  },

  onCustomizeStart: function(aWindow) {
    this._countEvent(["customize", "start"]);
    let durationMap = WINDOW_DURATION_MAP.get(aWindow);
    if (!durationMap) {
      durationMap = {};
      WINDOW_DURATION_MAP.set(aWindow, durationMap);
    }
    durationMap.customization = aWindow.performance.now();
  },

  onCustomizeEnd: function(aWindow) {
    let durationMap = WINDOW_DURATION_MAP.get(aWindow);
    if (durationMap && "customization" in durationMap) {
      let duration = aWindow.performance.now() - durationMap.customization;
      this._durations.customization.push(duration);
      delete durationMap.customization;
    }
  },
};

/**
 * Returns the id of the first ancestor of aNode that has an id. If aNode
 * has no parent, or no ancestor has an id, returns null.
 *
 * @param aNode the node to find the first ID'd ancestor of
 */
function getIDBasedOnFirstIDedAncestor(aNode) {
  while (!aNode.id) {
    aNode = aNode.parentNode;
    if (!aNode) {
      return null;
    }
  }

  return aNode.id;
}
