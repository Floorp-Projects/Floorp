/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module tracks each browser window and informs network module
 * the current selected tab's content outer window ID.
 */

var EXPORTED_SYMBOLS = ["BrowserWindowTracker"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

// Lazy getters
XPCOMUtils.defineLazyModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

// Constants
const TAB_EVENTS = ["TabBrowserInserted", "TabSelect"];
const WINDOW_EVENTS = ["activate", "unload"];
const DEBUG = false;

// Variables
var _lastTopBrowsingContextID = 0;
var _trackedWindows = [];

// Global methods
function debug(s) {
  if (DEBUG) {
    dump("-*- UpdateTopBrowsingContextIDHelper: " + s + "\n");
  }
}

function _updateCurrentBrowsingContextID(browser) {
  if (
    !browser.browsingContext ||
    browser.browsingContext.id === _lastTopBrowsingContextID ||
    browser.ownerGlobal != _trackedWindows[0]
  ) {
    return;
  }

  debug(
    "Current window uri=" +
      (browser.currentURI && browser.currentURI.spec) +
      " browsing context id=" +
      browser.browsingContext.id
  );

  _lastTopBrowsingContextID = browser.browsingContext.id;
  let idWrapper = Cc["@mozilla.org/supports-PRUint64;1"].createInstance(
    Ci.nsISupportsPRUint64
  );
  idWrapper.data = _lastTopBrowsingContextID;
  Services.obs.notifyObservers(
    idWrapper,
    "net:current-top-browsing-context-id"
  );
}

function _handleEvent(event) {
  switch (event.type) {
    case "TabBrowserInserted":
      if (
        event.target.ownerGlobal.gBrowser.selectedBrowser ===
        event.target.linkedBrowser
      ) {
        _updateCurrentBrowsingContextID(event.target.linkedBrowser);
      }
      break;
    case "TabSelect":
      _updateCurrentBrowsingContextID(event.target.linkedBrowser);
      break;
    case "activate":
      WindowHelper.onActivate(event.target);
      break;
    case "unload":
      WindowHelper.removeWindow(event.currentTarget);
      break;
  }
}

function _trackWindowOrder(window) {
  if (window.windowState == window.STATE_MINIMIZED) {
    let firstMinimizedWindow = _trackedWindows.findIndex(
      w => w.windowState == w.STATE_MINIMIZED
    );
    if (firstMinimizedWindow == -1) {
      firstMinimizedWindow = _trackedWindows.length;
    }
    _trackedWindows.splice(firstMinimizedWindow, 0, window);
  } else {
    _trackedWindows.unshift(window);
  }
}

function _untrackWindowOrder(window) {
  let idx = _trackedWindows.indexOf(window);
  if (idx >= 0) {
    _trackedWindows.splice(idx, 1);
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

    _trackWindowOrder(window);

    // Update the selected tab's content outer window ID.
    _updateCurrentBrowsingContextID(window.gBrowser.selectedBrowser);
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
  },

  onActivate(window) {
    // If this window was the last focused window, we don't need to do anything
    if (window == _trackedWindows[0]) {
      return;
    }

    _untrackWindowOrder(window);
    _trackWindowOrder(window);

    _updateCurrentBrowsingContextID(window.gBrowser.selectedBrowser);
  },
};

const BrowserWindowTracker = {
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
      if (
        !win.closed &&
        (options.allowPopups || win.toolbar.visible) &&
        (!("private" in options) ||
          lazy.PrivateBrowsingUtils.permanentPrivateBrowsing ||
          lazy.PrivateBrowsingUtils.isWindowPrivate(win) == options.private)
      ) {
        return win;
      }
    }
    return null;
  },

  windowCreated(browser) {
    if (browser === browser.ownerGlobal.gBrowser.selectedBrowser) {
      _updateCurrentBrowsingContextID(browser);
    }
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

  getAllVisibleTabs() {
    let tabs = [];
    for (let win of BrowserWindowTracker.orderedWindows) {
      for (let tab of win.gBrowser.visibleTabs) {
        // Only use tabs which are not discarded / unrestored
        if (tab.linkedPanel) {
          let { contentTitle, browserId } = tab.linkedBrowser;
          tabs.push({ contentTitle, browserId });
        }
      }
    }
    return tabs;
  },

  track(window) {
    return WindowHelper.addWindow(window);
  },

  getBrowserById(browserId) {
    for (let win of BrowserWindowTracker.orderedWindows) {
      for (let tab of win.gBrowser.visibleTabs) {
        if (tab.linkedPanel && tab.linkedBrowser.browserId === browserId) {
          return tab.linkedBrowser;
        }
      }
    }
    return null;
  },

  // For tests only, this function will remove this window from the list of
  // tracked windows. Please don't forget to add it back at the end of your
  // tests!
  untrackForTestsOnly(window) {
    return WindowHelper.removeWindow(window);
  },
};
