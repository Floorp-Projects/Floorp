/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is in charge of preloading 'new tab' pages for use when
 * the user opens a new tab.
 */

var EXPORTED_SYMBOLS = ["NewTabPagePreloading"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gAboutNewTabService: ["@mozilla.org/browser/aboutnewtab-service;1", "nsIAboutNewTabService"],
});

let NewTabPagePreloading = {
  // Maximum number of instances of a given page we'll preload at any time.
  // Because we preload about:newtab for normal windows, and about:privatebrowsing
  // for private ones, we could have 3 of each.
  MAX_COUNT: 3,

  // How many preloaded tabs we have, across all windows, for the private and non-private
  // case:
  browserCounts: {
    normal: 0,
    private: 0,
  },

  get enabled() {
    return this.prefEnabled && this.newTabEnabled &&
      !gAboutNewTabService.overridden;
  },

  /**
   * Create a browser in the right process type.
   */
  _createBrowser(win) {
    const {
      gBrowser,
      gMultiProcessBrowser,
      gFissionBrowser,
      BROWSER_NEW_TAB_URL,
    } = win;

    let remoteType =
      E10SUtils.getRemoteTypeForURI(BROWSER_NEW_TAB_URL,
                                    gMultiProcessBrowser,
                                    gFissionBrowser);
    let browser = gBrowser.createBrowser({ isPreloadBrowser: true, remoteType });
    gBrowser.preloadedBrowser = browser;

    let panel = gBrowser.getPanel(browser);
    gBrowser.tabpanels.appendChild(panel);

    if (remoteType != E10SUtils.NOT_REMOTE) {
      // For remote browsers, we need to make sure that the webProgress is
      // instantiated, otherwise the parent won't get informed about the state
      // of the preloaded browser until it gets attached to a tab.
      browser.webProgress;
    }
    return browser;
  },

  /**
   * Move the contents of a preload browser across to a different window.
   */
  _adoptBrowserFromOtherWindow(window) {
    let winPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    // Grab the least-recently-focused window with a preloaded browser:
    let oldWin = BrowserWindowTracker.orderedWindows.filter(w => {
      return winPrivate == PrivateBrowsingUtils.isWindowPrivate(w) &&
        w.gBrowser && w.gBrowser.preloadedBrowser;
    }).pop();
    if (!oldWin) {
      return null;
    }
    // Don't call getPreloadedBrowser because it'll consume the browser:
    let oldBrowser = oldWin.gBrowser.preloadedBrowser;
    oldWin.gBrowser.preloadedBrowser = null;

    let newBrowser = this._createBrowser(window);

    oldWin.gBrowser._outerWindowIDBrowserMap.delete(oldBrowser.outerWindowID);
    window.gBrowser._outerWindowIDBrowserMap.delete(newBrowser.outerWindowID);

    oldBrowser.swapBrowsers(newBrowser);

    // Switch outerWindowIDs for remote browsers.
    if (newBrowser.isRemoteBrowser) {
      newBrowser._outerWindowID = oldBrowser._outerWindowID;
    }

    window.gBrowser._outerWindowIDBrowserMap.set(newBrowser.outerWindowID, newBrowser);
    newBrowser.permanentKey = oldBrowser.permanentKey;

    oldWin.gBrowser.getPanel(oldBrowser).remove();
    return newBrowser;
  },

  maybeCreatePreloadedBrowser(window) {
    // If we're not enabled, have already got one, or are in a popup window,
    // don't bother creating a preload browser - there's no point.
    if (!this.enabled || window.gBrowser.preloadedBrowser ||
        !window.toolbar.visible) {
      return;
    }

    // Don't bother creating a preload browser if we're not in the top set of windows:
    let windowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    let countKey = windowPrivate ? "private" : "normal";
    let topWindows = BrowserWindowTracker.orderedWindows.filter(w =>
      PrivateBrowsingUtils.isWindowPrivate(w) == windowPrivate);
    if (topWindows.indexOf(window) >= this.MAX_COUNT) {
      return;
    }

    // If we're in the top set of windows, and we already have enough preloaded
    // tabs, don't create yet another one, just steal an existing one:
    if (this.browserCounts[countKey] >= this.MAX_COUNT) {
      let browser = this._adoptBrowserFromOtherWindow(window);
      // We can potentially get null here if we couldn't actually find another
      // browser to adopt from. This can be the case when there's a mix of
      // private and non-private windows, for instance.
      if (browser) {
        return;
      }
    }

    let browser = this._createBrowser(window);
    browser.loadURI(window.BROWSER_NEW_TAB_URL, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    browser.docShellIsActive = false;
    browser._urlbarFocused = true;

    // Make sure the preloaded browser is loaded with desired zoom level
    let tabURI = Services.io.newURI(window.BROWSER_NEW_TAB_URL);
    window.FullZoom.onLocationChange(tabURI, false, browser);

    this.browserCounts[countKey]++;
  },

  getPreloadedBrowser(window) {
    if (!this.enabled) {
      return null;
    }

    // The preloaded browser might be null.
    let browser = window.gBrowser.preloadedBrowser;

    // Consume the browser.
    window.gBrowser.preloadedBrowser = null;

    // Attach the nsIFormFillController now that we know the browser
    // will be used. If we do that before and the preloaded browser
    // won't be consumed until shutdown then we leak a docShell.
    // Also, we do not need to take care of attaching nsIFormFillControllers
    // in the case that the browser is remote, as remote browsers take
    // care of that themselves.
    if (browser) {
      let countKey = PrivateBrowsingUtils.isWindowPrivate(window) ? "private" : "normal";
      this.browserCounts[countKey]--;
      browser.setAttribute("preloadedState", "consumed");
      browser.setAttribute("autocompletepopup", "PopupAutoComplete");
    }

    return browser;
  },

  removePreloadedBrowser(window) {
    let browser = this.getPreloadedBrowser(window);
    if (browser) {
      window.gBrowser.getPanel(browser).remove();
    }
  },
};

XPCOMUtils.defineLazyPreferenceGetter(NewTabPagePreloading, "prefEnabled", "browser.newtab.preload", true);
XPCOMUtils.defineLazyPreferenceGetter(NewTabPagePreloading, "newTabEnabled", "browser.newtabpage.enabled", true);
