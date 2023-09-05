/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module tracks each browser window and informs network module
 * the current selected tab's content outer window ID.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

// Lazy getters

XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

ChromeUtils.defineESModuleGetters(lazy, {
  HomePage: "resource:///modules/HomePage.sys.mjs",
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

function topicObserved(observeTopic, checkFn) {
  return new Promise((resolve, reject) => {
    function observer(subject, topic, data) {
      try {
        if (checkFn && !checkFn(subject, data)) {
          return;
        }
        Services.obs.removeObserver(observer, topic);
        checkFn = null;
        resolve([subject, data]);
      } catch (ex) {
        Services.obs.removeObserver(observer, topic);
        checkFn = null;
        reject(ex);
      }
    }
    Services.obs.addObserver(observer, observeTopic);
  });
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

export const BrowserWindowTracker = {
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

    // Prevent leaks in case the window closes before we track it as an open
    // window.
    const topic = "browsing-context-discarded";
    const observer = (aSubject, aTopic, aData) => {
      if (window.browsingContext == aSubject) {
        let pending = this.pendingWindows.get(window);
        if (pending) {
          this.pendingWindows.delete(window);
          pending.deferred.resolve(window);
        }
        Services.obs.removeObserver(observer, topic);
      }
    };
    Services.obs.addObserver(observer, topic);
  },

  /**
   * A standard function for opening a new browser window.
   *
   * @param {Object} [options]
   *   Options for the new window.
   * @param {Window} [options.openerWindow]
   *   An existing browser window to open the new one from.
   * @param {boolean} [options.private]
   *   True to make the window a private browsing window.
   * @param {String} [options.features]
   *   Additional window features to give the new window.
   * @param {nsIArray | nsISupportsString} [options.args]
   *   Arguments to pass to the new window.
   * @param {boolean} [options.remote]
   *   A boolean indicating if the window should run remote browser tabs or
   *   not. If omitted, the window  will choose the profile default state.
   * @param {boolean} [options.fission]
   *   A boolean indicating if the window should run with fission enabled or
   *   not. If omitted, the window will choose the profile default state.
   *
   * @returns {Window}
   */
  openWindow({
    openerWindow = undefined,
    private: isPrivate = false,
    features = undefined,
    args = null,
    remote = undefined,
    fission = undefined,
  } = {}) {
    let telemetryObj = {};
    TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

    let windowFeatures = "chrome,dialog=no,all";
    if (features) {
      windowFeatures += `,${features}`;
    }
    let loadURIString;
    if (isPrivate && lazy.PrivateBrowsingUtils.enabled) {
      windowFeatures += ",private";
      if (!args && !lazy.PrivateBrowsingUtils.permanentPrivateBrowsing) {
        // Force the new window to load about:privatebrowsing instead of the
        // default home page.
        loadURIString = "about:privatebrowsing";
      }
    } else {
      windowFeatures += ",non-private";
    }
    if (!args) {
      loadURIString ??= lazy.BrowserHandler.defaultArgs;
      args = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      args.data = loadURIString;
    }

    if (remote) {
      windowFeatures += ",remote";
    } else if (remote === false) {
      windowFeatures += ",non-remote";
    }

    if (fission) {
      windowFeatures += ",fission";
    } else if (fission === false) {
      windowFeatures += ",non-fission";
    }

    // If the opener window is maximized, we want to skip the animation, since
    // we're going to be taking up most of the screen anyways, and we want to
    // optimize for showing the user a useful window as soon as possible.
    if (openerWindow?.windowState == openerWindow?.STATE_MAXIMIZED) {
      windowFeatures += ",suppressanimation";
    }

    let win = Services.ww.openWindow(
      openerWindow,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      windowFeatures,
      args
    );
    this.registerOpeningWindow(win, isPrivate);

    win.addEventListener(
      "MozAfterPaint",
      () => {
        TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
        if (
          Services.prefs.getIntPref("browser.startup.page") == 1 &&
          loadURIString == lazy.HomePage.get()
        ) {
          // A notification for when a user has triggered their homepage. This
          // is used to display a doorhanger explaining that an extension has
          // modified the homepage, if necessary.
          Services.obs.notifyObservers(win, "browser-open-homepage-start");
        }
      },
      { once: true }
    );

    return win;
  },

  /**
   * Async version of `openWindow` waiting for delayed startup of the new
   * window before returning.
   *
   * @param {Object} [options]
   *   Options for the new window. See `openWindow` for details.
   *
   * @returns {Window}
   */
  async promiseOpenWindow(options) {
    let win = this.openWindow(options);
    await topicObserved(
      "browser-delayed-startup-finished",
      subject => subject == win
    );
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
