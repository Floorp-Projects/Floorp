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

function _updateCurrentContentOuterWindowID(browser) {
  if (!browser.outerWindowID ||
      browser.outerWindowID === _lastTopLevelWindowID) {
    return;
  }

  debug("Current window uri=" + browser.currentURI.spec +
        " id=" + browser.outerWindowID);

  _lastTopLevelWindowID = browser.outerWindowID;
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

function _handleMessage(message) {
  let browser = message.target;
  if (message.name === "Browser:Init" &&
      browser === browser.ownerGlobal.gBrowser.selectedBrowser) {
    _updateCurrentContentOuterWindowID(browser);
  }
}

// Methods that impact a window. Put into single object for organization.
var WindowHelper = {
  addWindow(window) {
    // Add event listeners
    TAB_EVENTS.forEach(function(event) {
      window.gBrowser.tabContainer.addEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      window.addEventListener(event, _handleEvent);
    });

    let messageManager = window.getGroupMessageManager("browsers");
    messageManager.addMessageListener("Browser:Init", _handleMessage);

    // This gets called AFTER activate event, so if this is the focused window
    // we want to activate it.
    if (window == _focusManager.activeWindow)
      this.handleFocusedWindow(window);

    // Update the selected tab's content outer window ID.
    _updateCurrentContentOuterWindowID(window.gBrowser.selectedBrowser);
  },

  removeWindow(window) {
    if (window == _lastFocusedWindow)
      _lastFocusedWindow = null;

    // Remove the event listeners
    TAB_EVENTS.forEach(function(event) {
      window.gBrowser.tabContainer.removeEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      window.removeEventListener(event, _handleEvent);
    });

    let messageManager = window.getGroupMessageManager("browsers");
    messageManager.removeMessageListener("Browser:Init", _handleMessage);
  },

  onActivate(window, hasFocus) {
    // If this window was the last focused window, we don't need to do anything
    if (window == _lastFocusedWindow)
      return;

    this.handleFocusedWindow(window);

    _updateCurrentContentOuterWindowID(window.gBrowser.selectedBrowser);
  },

  handleFocusedWindow(window) {
    // window is now focused
    _lastFocusedWindow = window;
  },

  getTopWindow(options) {
    let checkPrivacy = typeof options == "object" &&
                       "private" in options;

    let allowPopups = typeof options == "object" && !!options.allowPopups;

    function isSuitableBrowserWindow(win) {
      return (!win.closed &&
              (allowPopups || win.toolbar.visible) &&
              (!checkPrivacy ||
               PrivateBrowsingUtils.permanentPrivateBrowsing ||
               PrivateBrowsingUtils.isWindowPrivate(win) == options.private));
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
   * @param options an object accepting the arguments for the search.
   *        * private: true to restrict the search to private windows
   *            only, false to restrict the search to non-private only.
   *            Omit the property to search in both groups.
   *        * allowPopups: true if popup windows are permissable.
   */
  getTopWindow(options) {
    return WindowHelper.getTopWindow(options);
  },

  track(window) {
    return WindowHelper.addWindow(window);
  }
};
