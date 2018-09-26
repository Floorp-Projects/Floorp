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
XPCOMUtils.defineLazyModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

// Constants
const TAB_EVENTS = ["TabBrowserInserted", "TabSelect"];
const WINDOW_EVENTS = ["activate", "sizemodechange", "unload"];
const DEBUG = false;

// Variables
var _lastTopLevelWindowID = 0;
var _trackedWindows = [];

// Global methods
function debug(s) {
  if (DEBUG) {
    dump("-*- UpdateTopLevelContentWindowIDHelper: " + s + "\n");
  }
}

function _updateCurrentContentOuterWindowID(browser) {
  if (!browser.outerWindowID ||
      browser.outerWindowID === _lastTopLevelWindowID ||
      browser.ownerGlobal != _trackedWindows[0]) {
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

function _handleEvent(event) {
  switch (event.type) {
    case "TabBrowserInserted":
      if (event.target.ownerGlobal.gBrowser.selectedBrowser === event.target.linkedBrowser) {
        _updateCurrentContentOuterWindowID(event.target.linkedBrowser);
      }
      break;
    case "TabSelect":
      _updateCurrentContentOuterWindowID(event.target.linkedBrowser);
      break;
    case "activate":
      WindowHelper.onActivate(event.target);
      break;
    case "sizemodechange":
      WindowHelper.onSizemodeChange(event.target);
      break;
    case "unload":
      WindowHelper.removeWindow(event.currentTarget);
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

function _trackWindowOrder(window) {
  _trackedWindows.splice(window.windowState == window.STATE_MINIMIZED ?
    _trackedWindows.length - 1 : 0, 0, window);
}

function _untrackWindowOrder(window) {
  let idx = _trackedWindows.indexOf(window);
  if (idx >= 0)
    _trackedWindows.splice(idx, 1);
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

    _trackWindowOrder(window);

    // Update the selected tab's content outer window ID.
    _updateCurrentContentOuterWindowID(window.gBrowser.selectedBrowser);
  },

  removeWindow(window) {
    _untrackWindowOrder(window);

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
    if (window == _trackedWindows[0])
      return;

    _untrackWindowOrder(window);
    _trackWindowOrder(window);

    _updateCurrentContentOuterWindowID(window.gBrowser.selectedBrowser);
  },

  onSizemodeChange(window) {
    if (window.windowState == window.STATE_MINIMIZED) {
      // Make sure to have the minimized window at the end of the list.
      _untrackWindowOrder(window);
      _trackedWindows.push(window);
    }
  },
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
  getTopWindow(options = {}) {
    for (let win of _trackedWindows) {
      if (!win.closed &&
          (options.allowPopups || win.toolbar.visible) &&
          (!("private" in options) ||
           PrivateBrowsingUtils.permanentPrivateBrowsing ||
           PrivateBrowsingUtils.isWindowPrivate(win) == options.private)) {
        return win;
      }
    }
    return null;
  },

  /**
   * Number of currently open browser windows.
   */
  get windowCount() {
    return _trackedWindows.length;
  },

  /**
   * Array of browser windows ordered by z-index, in reverse order.
   * This means that the top-most browser window will be the first item.
   */
  get orderedWindows() {
    // Clone the windows array immediately as it may change during iteration,
    // we'd rather have an outdated order than skip/revisit windows.
    return [..._trackedWindows];
  },

  track(window) {
    return WindowHelper.addWindow(window);
  },
};
