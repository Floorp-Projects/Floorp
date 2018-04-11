/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module tracks each browser window and informs network module
 * the current selected tab's content outer window ID.
 */

var EXPORTED_SYMBOLS = ["BrowserWindowTracker"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// Lazy getters
XPCOMUtils.defineLazyServiceGetter(this, "_focusManager",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm"
});

// Constants
const TAB_EVENTS = ["TabBrowserInserted", "TabSelect"];
const WINDOW_EVENTS = ["activate", "unload"];
const DEBUG = false;

// Variables
var _lastFocusedWindow = null;
var _lastTopLevelWindowID = 0;

// Global methods
function debug(s) {
  if (DEBUG) {
    dump("-*- UpdateTopLevelContentWindowIDHelper: " + s + "\n");
  }
}

function _updateCurrentContentOuterWindowID(aBrowser) {
  if (!aBrowser.outerWindowID ||
      aBrowser.outerWindowID === _lastTopLevelWindowID) {
    return;
  }

  debug("Current window uri=" + aBrowser.currentURI.spec +
        " id=" + aBrowser.outerWindowID);

  _lastTopLevelWindowID = aBrowser.outerWindowID;
  let windowIDWrapper = Cc["@mozilla.org/supports-PRUint64;1"]
                          .createInstance(Ci.nsISupportsPRUint64);
  windowIDWrapper.data = _lastTopLevelWindowID;
  Services.obs.notifyObservers(windowIDWrapper,
                               "net:current-toplevel-outer-content-windowid");
}

function _handleEvent(aEvent) {
  switch (aEvent.type) {
    case "TabBrowserInserted":
      if (aEvent.target.ownerGlobal.gBrowser.selectedBrowser === aEvent.target.linkedBrowser) {
        _updateCurrentContentOuterWindowID(aEvent.target.linkedBrowser);
      }
      break;
    case "TabSelect":
      _updateCurrentContentOuterWindowID(aEvent.target.linkedBrowser);
      break;
    case "activate":
      WindowHelper.onActivate(aEvent.target);
      break;
    case "unload":
      WindowHelper.removeWindow(aEvent.currentTarget);
      break;
  }
}

function _handleMessage(aMessage) {
  let browser = aMessage.target;
  if (aMessage.name === "Browser:Init" &&
      browser === browser.ownerGlobal.gBrowser.selectedBrowser) {
    _updateCurrentContentOuterWindowID(browser);
  }
}

// Methods that impact a window. Put into single object for organization.
var WindowHelper = {
  addWindow: function NP_WH_addWindow(aWindow) {
    // Add event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.addEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.addEventListener(event, _handleEvent);
    });

    let messageManager = aWindow.getGroupMessageManager("browsers");
    messageManager.addMessageListener("Browser:Init", _handleMessage);

    // This gets called AFTER activate event, so if this is the focused window
    // we want to activate it.
    if (aWindow == _focusManager.activeWindow)
      this.handleFocusedWindow(aWindow);

    // Update the selected tab's content outer window ID.
    _updateCurrentContentOuterWindowID(aWindow.gBrowser.selectedBrowser);
  },

  removeWindow: function NP_WH_removeWindow(aWindow) {
    if (aWindow == _lastFocusedWindow)
      _lastFocusedWindow = null;

    // Remove the event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.removeEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.removeEventListener(event, _handleEvent);
    });

    let messageManager = aWindow.getGroupMessageManager("browsers");
    messageManager.removeMessageListener("Browser:Init", _handleMessage);
  },

  onActivate: function NP_WH_onActivate(aWindow, aHasFocus) {
    // If this window was the last focused window, we don't need to do anything
    if (aWindow == _lastFocusedWindow)
      return;

    this.handleFocusedWindow(aWindow);

    _updateCurrentContentOuterWindowID(aWindow.gBrowser.selectedBrowser);
  },

  handleFocusedWindow: function NP_WH_handleFocusedWindow(aWindow) {
    // aWindow is now focused
    _lastFocusedWindow = aWindow;
  },

  getMostRecentBrowserWindow: function RW_getMostRecentBrowserWindow(aOptions) {
    let checkPrivacy = typeof aOptions == "object" &&
                       "private" in aOptions;

    let allowPopups = typeof aOptions == "object" && !!aOptions.allowPopups;

    function isSuitableBrowserWindow(win) {
      return (!win.closed &&
              (allowPopups || win.toolbar.visible) &&
              (!checkPrivacy ||
               PrivateBrowsingUtils.permanentPrivateBrowsing ||
               PrivateBrowsingUtils.isWindowPrivate(win) == aOptions.private));
    }

    let broken_wm_z_order =
      AppConstants.platform != "macosx" && AppConstants.platform != "win";

    if (broken_wm_z_order) {
      let win = Services.wm.getMostRecentWindow("navigator:browser");

      // if we're lucky, this isn't a popup, and we can just return this
      if (win && !isSuitableBrowserWindow(win)) {
        win = null;
        let windowList = Services.wm.getEnumerator("navigator:browser");
        // this is oldest to newest, so this gets a bit ugly
        while (windowList.hasMoreElements()) {
          let nextWin = windowList.getNext();
          if (isSuitableBrowserWindow(nextWin))
            win = nextWin;
        }
      }
      return win;
    }
    let windowList = Services.wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
    while (windowList.hasMoreElements()) {
      let win = windowList.getNext();
      if (isSuitableBrowserWindow(win))
        return win;
    }
    return null;
  }
};

this.BrowserWindowTracker = {
  /**
   * Get the most recent browser window.
   *
   * @param aOptions an object accepting the arguments for the search.
   *        * private: true to restrict the search to private windows
   *            only, false to restrict the search to non-private only.
   *            Omit the property to search in both groups.
   *        * allowPopups: true if popup windows are permissable.
   */
  getMostRecentBrowserWindow(options) {
    return WindowHelper.getMostRecentBrowserWindow(options);
  },

  track(window) {
    return WindowHelper.addWindow(window);
  }
};
