/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
/**
 * This constant is chosen to be large enough for a portal recheck to complete,
 * and small enough that the delay in opening a tab isn't too noticeable.
 * Please see comments for _delayedAddCaptivePortalTab for more details.
 */
const PORTAL_RECHECK_DELAY_MS = 150;

this.EXPORTED_SYMBOLS = [ "CaptivePortalWatcher" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cps",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

this.CaptivePortalWatcher = {
  // This holds a weak reference to the captive portal tab so that we
  // don't leak it if the user closes it.
  _captivePortalTab: null,

  _initialized: false,

  /**
   * If a portal is detected when we don't have focus, we first wait for focus
   * and then add the tab after a small delay. This is set to true while we wait
   * so that in the unlikely event that we receive another notification while
   * waiting, we can avoid adding a second tab.
   */
  _waitingToAddTab: false,

  get canonicalURL() {
    return Services.prefs.getCharPref("captivedetect.canonicalURL");
  },

  init() {
    Services.obs.addObserver(this, "captive-portal-login", false);
    Services.obs.addObserver(this, "captive-portal-login-abort", false);
    Services.obs.addObserver(this, "captive-portal-login-success", false);
    this._initialized = true;
    if (cps.state == cps.LOCKED_PORTAL) {
      // A captive portal has already been detected.
      this._addCaptivePortalTab();
    }
  },

  uninit() {
    if (!this._initialized) {
      return;
    }
    Services.obs.removeObserver(this, "captive-portal-login");
    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");
  },

  observe(subject, topic, data) {
    switch(topic) {
      case "captive-portal-login":
        this._addCaptivePortalTab();
        break;
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        this._captivePortalGone();
        break;
      case "xul-window-visible":
        this._delayedAddCaptivePortalTab();
        break;
    }
  },

  _addCaptivePortalTab() {
    if (this._waitingToAddTab) {
      return;
    }

    let win = RecentWindow.getMostRecentBrowserWindow();
    // If there's no browser window or none have focus, open and show the
    // tab when we regain focus. This is so that if a different application was
    // focused, when the user (re-)focuses a browser window, we open the tab
    // immediately in that window so they can login before continuing to browse.
    if (!win || !win.document.hasFocus()) {
      this._waitingToAddTab = true;
      Services.obs.addObserver(this, "xul-window-visible", false);
      return;
    }

    // The browser is in use - add the tab without selecting it.
    let tab = win.gBrowser.addTab(this.canonicalURL);
    this._captivePortalTab = Cu.getWeakReference(tab);
    return;
  },

  /**
   * Called after we regain focus if we detect a portal while a browser window
   * doesn't have focus. Triggers a portal recheck to reaffirm state, and adds
   * the tab if needed after a short delay to allow the recheck to complete.
   */
  _delayedAddCaptivePortalTab() {
    if (!this._waitingToAddTab) {
      return;
    }

    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win.document.hasFocus()) {
      // The document that got focused was not in a browser window.
      return;
    }
    Services.obs.removeObserver(this, "xul-window-visible");

    // Trigger a portal recheck. The user may have logged into the portal via
    // another client, or changed networks.
    let lastChecked = cps.lastChecked;
    cps.recheckCaptivePortal();

    // We wait for PORTAL_RECHECK_DELAY_MS after the trigger.
    // - If the portal is no longer locked, we don't need to add a tab.
    // - If it is, the delay is chosen to not be extremely noticeable.
    setTimeout(() => {
      this._waitingToAddTab = false;
      if (cps.state != cps.LOCKED_PORTAL) {
        // We're free of the portal!
        return;
      }

      let tab = win.gBrowser.addTab(this.canonicalURL);
      // Focus the tab only if the recheck has completed, i.e. we're sure
      // that the portal is still locked. This way, if the recheck completes
      // after we add the tab and we're free of the portal, the tab contents
      // won't flicker.
      if (cps.lastChecked != lastChecked) {
        win.gBrowser.selectedTab = tab;
      }

      this._captivePortalTab = Cu.getWeakReference(tab);
    }, PORTAL_RECHECK_DELAY_MS);
  },

  _captivePortalGone() {
    if (this._waitingToAddTab) {
      Services.obs.removeObserver(this, "xul-window-visible");
      this._waitingToAddTab = false;
    }

    if (!this._captivePortalTab) {
      return;
    }

    let tab = this._captivePortalTab.get();
    // In all the cases below, we want to stop treating the tab as a
    // captive portal tab.
    this._captivePortalTab = null;

    // Check parentNode in case the object hasn't been gc'd yet.
    if (!tab || tab.closing || !tab.parentNode) {
      // User has closed the tab already.
      return;
    }

    let tabbrowser = tab.ownerDocument.defaultView.gBrowser;

    // If after the login, the captive portal has redirected to some other page,
    // leave it open if the tab has focus.
    if (tab.linkedBrowser.currentURI.spec != this.canonicalURL &&
        tabbrowser.selectedTab == tab) {
      return;
    }

    // Remove the tab.
    tabbrowser.removeTab(tab);
  },
};
