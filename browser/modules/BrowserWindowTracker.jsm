/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module tracks each browser window and informs network module
 * the current selected tab's content outer window ID.
 */

var EXPORTED_SYMBOLS = ["BrowserWindowTracker"];

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const lazy = {};

// Lazy getters
ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

// Constants
const TAB_EVENTS = ["TabBrowserInserted", "TabSelect"];
const WINDOW_EVENTS = ["activate", "unload"];
const DEBUG = false;

// Variables
let _lastCurrentBrowserId = 0;
let _trackedWindows = [];

// Global methods
function debug(s) {
  if (DEBUG) {
    dump("-*- UpdateBrowserIDHelper: " + s + "\n");
  }
}

function _updateCurrentBrowserId(browser) {
  if (
    !browser.browserId ||
    browser.browserId === _lastCurrentBrowserId ||
    browser.ownerGlobal != _trackedWindows[0]
  ) {
    return;
  }

  // Guard on DEBUG here because materializing a long data URI into
  // a JS string for concatenation is not free.
  if (DEBUG) {
    debug(
      `Current window uri=${browser.currentURI?.spec} browser id=${browser.browserId}`
    );
  }

  _lastCurrentBrowserId = browser.browserId;
  let idWrapper = Cc["@mozilla.org/supports-PRUint64;1"].createInstance(
    Ci.nsISupportsPRUint64
  );
  idWrapper.data = _lastCurrentBrowserId;
  Services.obs.notifyObservers(idWrapper, "net:current-browser-id");
}

function _handleEvent(event) {
  switch (event.type) {
    case "TabBrowserInserted":
      if (
        event.target.ownerGlobal.gBrowser.selectedBrowser ===
        event.target.linkedBrowser
      ) {
        _updateCurrentBrowserId(event.target.linkedBrowser);
      }
      break;
    case "TabSelect":
      _updateCurrentBrowserId(event.target.linkedBrowser);
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
    TAB_EVENTS.forEach(function (event) {
      window.gBrowser.tabContainer.addEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function (event) {
      window.addEventListener(event, _handleEvent);
    });

    _trackWindowOrder(window);

    // Update the selected tab's content outer window ID.
    _updateCurrentBrowserId(window.gBrowser.selectedBrowser);
  },

  removeWindow(window) {
    _untrackWindowOrder(window);

    // Remove the event listeners
    TAB_EVENTS.forEach(function (event) {
      window.gBrowser.tabContainer.removeEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function (event) {
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

    _updateCurrentBrowserId(window.gBrowser.selectedBrowser);
  },
};

const BrowserWindowTracker = {
  pendingWindows: new Map(),

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

      // Floorp Injections
      var { FloorpServices } = ChromeUtils.importESModule("resource:///modules/FloorpServices.sys.mjs");
      let isFloorpSpecialWindow = FloorpServices.wm.IsFloorpSpecialWindow(win);
      // End Floorp Injections

      if (
        !win.closed && !isFloorpSpecialWindow &&
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

  /**
   * Get a window that is in the process of loading. Only supports windows
   * opened via the `openWindow` function in this module or that have been
   * registered with the `registerOpeningWindow` function.
   *
   * @param {Object} options
   *   Options for the search.
   * @param {boolean} [options.private]
   *   true to restrict the search to private windows only, false to restrict
   *   the search to non-private only. Omit the property to search in both
   *   groups.
   *
   * @returns {Promise<Window> | null}
   */
  getPendingWindow(options = {}) {
    for (let pending of this.pendingWindows.values()) {
      if (
        !("private" in options) ||
        lazy.PrivateBrowsingUtils.permanentPrivateBrowsing ||
        pending.isPrivate == options.private
      ) {
        return pending.deferred.promise;
      }
    }
    return null;
  },

  /**
   * Registers a browser window that is in the process of opening. Normally it
   * would be preferable to use the standard method for opening the window from
   * this module.
   *
   * @param {Window} window
   *   The opening window.
   * @param {boolean} isPrivate
   *   Whether the opening window is a private browsing window.
   */
  registerOpeningWindow(window, isPrivate) {
    let deferred = lazy.PromiseUtils.defer();

    this.pendingWindows.set(window, {
      isPrivate,
      deferred,
    });
  },

  /**
   * A standard function for opening a new browser window.
   *
   * @param {Object} [options]
   *   Options for the new window.
   * @param {boolean} [options.private]
   *   True to make the window a private browsing window.
   * @param {String} [options.features]
   *   Additional window features to give the new window.
   * @param {nsIArray | nsISupportsString} [options.args]
   *   Arguments to pass to the new window.
   *
   * @returns {Window}
   */
  openWindow({
    private: isPrivate = false,
    features = undefined,
    args = null,
  } = {}) {
    let windowFeatures = "chrome,dialog=no,all";
    if (features) {
      windowFeatures += `,${features}`;
    }
    if (isPrivate) {
      windowFeatures += ",private";
    }

    let win = Services.ww.openWindow(
      null,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      windowFeatures,
      args
    );
    this.registerOpeningWindow(win, isPrivate);
    return win;
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
    let pending = this.pendingWindows.get(window);
    if (pending) {
      this.pendingWindows.delete(window);
      // Waiting for delayed startup to complete ensures that this new window
      // has started loading its initial urls.
      window.delayedStartupPromise.then(() => pending.deferred.resolve(window));
    }

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
