/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "CaptivePortalWatcher" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cps",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

this.CaptivePortalWatcher = {
  /**
   * This constant is chosen to be large enough for a portal recheck to complete,
   * and small enough that the delay in opening a tab isn't too noticeable.
   * Please see comments for _delayedCaptivePortalDetected for more details.
   */
  PORTAL_RECHECK_DELAY_MS: 150,

  // This is the value used to identify the captive portal notification.
  PORTAL_NOTIFICATION_VALUE: "captive-portal-detected",

  // This holds a weak reference to the captive portal tab so that we
  // don't leak it if the user closes it.
  _captivePortalTab: null,

  // This holds a weak reference to the captive portal notification.
  _captivePortalNotification: null,

  _initialized: false,

  /**
   * If a portal is detected when we don't have focus, we first wait for focus
   * and then add the tab if, after a recheck, the portal is still active. This
   * is set to true while we wait so that in the unlikely event that we receive
   * another notification while waiting, we don't do things twice.
   */
  _delayedCaptivePortalDetectedInProgress: false,

  // In the situation above, this is set to true while we wait for the recheck.
  // This flag exists so that tests can appropriately simulate a recheck.
  _waitingForRecheck: false,

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
      this._captivePortalDetected();
      return;
    }

    cps.recheckCaptivePortal();
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
    switch (topic) {
      case "captive-portal-login":
        this._captivePortalDetected();
        break;
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        this._captivePortalGone();
        break;
      case "xul-window-visible":
        this._delayedCaptivePortalDetected();
        break;
    }
  },

  _captivePortalDetected() {
    if (this._delayedCaptivePortalDetectedInProgress) {
      return;
    }

    let win = RecentWindow.getMostRecentBrowserWindow();
    // If there's no browser window or none have focus, open and show the
    // tab when we regain focus. This is so that if a different application was
    // focused, when the user (re-)focuses a browser window, we open the tab
    // immediately in that window so they can login before continuing to browse.
    if (!win || win != Services.ww.activeWindow) {
      this._delayedCaptivePortalDetectedInProgress = true;
      Services.obs.addObserver(this, "xul-window-visible", false);
      return;
    }

    this._showNotification(win);
  },

  _ensureCaptivePortalTab(win) {
    let tab;
    if (this._captivePortalTab) {
      tab = this._captivePortalTab.get();
    }

    // If the tab is gone or going, we need to open a new one.
    if (!tab || tab.closing || !tab.parentNode) {
      tab = win.gBrowser.addTab(this.canonicalURL,
                                { ownerTab: win.gBrowser.selectedTab });
      this._captivePortalTab = Cu.getWeakReference(tab);
    }

    win.gBrowser.selectedTab = tab;
  },

  /**
   * Called after we regain focus if we detect a portal while a browser window
   * doesn't have focus. Triggers a portal recheck to reaffirm state, and adds
   * the tab if needed after a short delay to allow the recheck to complete.
   */
  _delayedCaptivePortalDetected() {
    if (!this._delayedCaptivePortalDetectedInProgress) {
      return;
    }

    let win = RecentWindow.getMostRecentBrowserWindow();
    if (win != Services.ww.activeWindow) {
      // The window that got focused was not a browser window.
      return;
    }
    Services.obs.removeObserver(this, "xul-window-visible");

    // Trigger a portal recheck. The user may have logged into the portal via
    // another client, or changed networks.
    cps.recheckCaptivePortal();
    this._waitingForRecheck = true;
    let requestTime = Date.now();

    let self = this;
    Services.obs.addObserver(function observer() {
      let time = Date.now() - requestTime;
      Services.obs.removeObserver(observer, "captive-portal-check-complete");
      self._waitingForRecheck = false;
      self._delayedCaptivePortalDetectedInProgress = false;
      if (cps.state != cps.LOCKED_PORTAL) {
        // We're free of the portal!
        return;
      }

      self._showNotification(win);
      if (time <= self.PORTAL_RECHECK_DELAY_MS) {
        // The amount of time elapsed since we requested a recheck (i.e. since
        // the browser window was focused) was small enough that we can add and
        // focus a tab with the login page with no noticeable delay.
        self._ensureCaptivePortalTab(win);
      }
    }, "captive-portal-check-complete", false);
  },

  _captivePortalGone() {
    if (this._delayedCaptivePortalDetectedInProgress) {
      Services.obs.removeObserver(this, "xul-window-visible");
      this._delayedCaptivePortalDetectedInProgress = false;
    }

    this._removeNotification();

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

    let tabbrowser = tab.ownerGlobal.gBrowser;

    // If after the login, the captive portal has redirected to some other page,
    // leave it open if the tab has focus.
    if (tab.linkedBrowser.currentURI.spec != this.canonicalURL &&
        tabbrowser.selectedTab == tab) {
      return;
    }

    // Remove the tab.
    tabbrowser.removeTab(tab);
  },

  get _browserBundle() {
    delete this._browserBundle;
    return this._browserBundle =
      Services.strings.createBundle("chrome://browser/locale/browser.properties");
  },

  handleEvent(aEvent) {
    if (aEvent.type != "TabSelect" || !this._captivePortalTab || !this._captivePortalNotification) {
      return;
    }

    let tab = this._captivePortalTab.get();
    let n = this._captivePortalNotification.get();
    if (!tab || !n) {
      return;
    }

    let doc = tab.ownerDocument;
    let button = n.querySelector("button.notification-button");
    if (doc.defaultView.gBrowser.selectedTab == tab) {
      button.style.visibility = "hidden";
    } else {
      button.style.visibility = "visible";
    }
  },

  _showNotification(win) {
    let buttons = [
      {
        label: this._browserBundle.GetStringFromName("captivePortal.showLoginPage"),
        callback: () => {
          this._ensureCaptivePortalTab(win);

          // Returning true prevents the notification from closing.
          return true;
        },
        isDefault: true,
      },
    ];

    let message = this._browserBundle.GetStringFromName("captivePortal.infoMessage2");

    let closeHandler = (aEventName) => {
      if (aEventName != "removed") {
        return;
      }
      win.gBrowser.tabContainer.removeEventListener("TabSelect", this);
    };

    let nb = win.document.getElementById("high-priority-global-notificationbox");
    let n = nb.appendNotification(message, this.PORTAL_NOTIFICATION_VALUE, "",
                                  nb.PRIORITY_INFO_MEDIUM, buttons, closeHandler);

    this._captivePortalNotification = Cu.getWeakReference(n);

    win.gBrowser.tabContainer.addEventListener("TabSelect", this);
  },

  _removeNotification() {
    if (!this._captivePortalNotification)
      return;
    let n = this._captivePortalNotification.get();
    this._captivePortalNotification = null;
    if (!n || !n.parentNode) {
      return;
    }
    n.close();
  },
};
