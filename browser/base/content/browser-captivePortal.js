/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyServiceGetter(this, "cps",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

var CaptivePortalWatcher = {
  /**
   * This constant is chosen to be large enough for a portal recheck to complete,
   * and small enough that the delay in opening a tab isn't too noticeable.
   * Please see comments for _delayedCaptivePortalDetected for more details.
   */
  PORTAL_RECHECK_DELAY_MS: Preferences.get("captivedetect.portalRecheckDelayMS", 500),

  // This is the value used to identify the captive portal notification.
  PORTAL_NOTIFICATION_VALUE: "captive-portal-detected",

  // This holds a weak reference to the captive portal tab so that we
  // don't leak it if the user closes it.
  _captivePortalTab: null,

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

  get _captivePortalNotification() {
    let nb = document.getElementById("high-priority-global-notificationbox");
    return nb.getNotificationWithValue(this.PORTAL_NOTIFICATION_VALUE);
  },

  get canonicalURL() {
    return Services.prefs.getCharPref("captivedetect.canonicalURL");
  },

  get _browserBundle() {
    delete this._browserBundle;
    return this._browserBundle =
      Services.strings.createBundle("chrome://browser/locale/browser.properties");
  },

  init() {
    Services.obs.addObserver(this, "captive-portal-login");
    Services.obs.addObserver(this, "captive-portal-login-abort");
    Services.obs.addObserver(this, "captive-portal-login-success");

    if (cps.state == cps.LOCKED_PORTAL) {
      // A captive portal has already been detected.
      this._captivePortalDetected();

      // Automatically open a captive portal tab if there's no other browser window.
      let windows = Services.wm.getEnumerator("navigator:browser");
      if (windows.getNext() == window && !windows.hasMoreElements()) {
        this.ensureCaptivePortalTab();
      }
    }

    cps.recheckCaptivePortal();
  },

  uninit() {
    Services.obs.removeObserver(this, "captive-portal-login");
    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");


    if (this._delayedCaptivePortalDetectedInProgress) {
      Services.obs.removeObserver(this, "xul-window-visible");
    }
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
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
    // If no browser window has focus, open and show the tab when we regain focus.
    // This is so that if a different application was focused, when the user
    // (re-)focuses a browser window, we open the tab immediately in that window
    // so they can log in before continuing to browse.
    if (win != Services.ww.activeWindow) {
      this._delayedCaptivePortalDetectedInProgress = true;
      Services.obs.addObserver(this, "xul-window-visible");
    }

    this._showNotification();
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
    this._delayedCaptivePortalDetectedInProgress = false;

    if (win != window) {
      // Some other browser window got focus, we don't have to do anything.
      return;
    }
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
      if (cps.state != cps.LOCKED_PORTAL) {
        // We're free of the portal!
        return;
      }

      if (time <= self.PORTAL_RECHECK_DELAY_MS) {
        // The amount of time elapsed since we requested a recheck (i.e. since
        // the browser window was focused) was small enough that we can add and
        // focus a tab with the login page with no noticeable delay.
        self.ensureCaptivePortalTab();
      }
    }, "captive-portal-check-complete");
  },

  _captivePortalGone() {
    if (this._delayedCaptivePortalDetectedInProgress) {
      Services.obs.removeObserver(this, "xul-window-visible");
      this._delayedCaptivePortalDetectedInProgress = false;
    }

    this._removeNotification();
  },

  handleEvent(aEvent) {
    if (aEvent.type != "TabSelect" || !this._captivePortalTab || !this._captivePortalNotification) {
      return;
    }

    let tab = this._captivePortalTab.get();
    let n = this._captivePortalNotification;
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

  _showNotification() {
    let buttons = [
      {
        label: this._browserBundle.GetStringFromName("captivePortal.showLoginPage2"),
        callback: () => {
          this.ensureCaptivePortalTab();

          // Returning true prevents the notification from closing.
          return true;
        },
        isDefault: true,
      },
    ];

    let message = this._browserBundle.GetStringFromName("captivePortal.infoMessage3");

    let closeHandler = (aEventName) => {
      if (aEventName != "removed") {
        return;
      }
      gBrowser.tabContainer.removeEventListener("TabSelect", this);
    };

    let nb = document.getElementById("high-priority-global-notificationbox");
    nb.appendNotification(message, this.PORTAL_NOTIFICATION_VALUE, "",
                          nb.PRIORITY_INFO_MEDIUM, buttons, closeHandler);

    gBrowser.tabContainer.addEventListener("TabSelect", this);
  },

  _removeNotification() {
    let n = this._captivePortalNotification;
    if (!n || !n.parentNode) {
      return;
    }
    n.close();
  },

  ensureCaptivePortalTab() {
    let tab;
    if (this._captivePortalTab) {
      tab = this._captivePortalTab.get();
    }

    // If the tab is gone or going, we need to open a new one.
    if (!tab || tab.closing || !tab.parentNode) {
      tab = gBrowser.addTab(this.canonicalURL, { ownerTab: gBrowser.selectedTab });
      this._captivePortalTab = Cu.getWeakReference(tab);
    }

    gBrowser.selectedTab = tab;

    let canonicalURI = makeURI(this.canonicalURL);

    // When we are no longer captive, close the tab if it's at the canonical URL.
    let tabCloser = () => {
      Services.obs.removeObserver(tabCloser, "captive-portal-login-abort");
      Services.obs.removeObserver(tabCloser, "captive-portal-login-success");
      if (!tab || tab.closing || !tab.parentNode || !tab.linkedBrowser ||
          !tab.linkedBrowser.currentURI.equalsExceptRef(canonicalURI)) {
        return;
      }
      gBrowser.removeTab(tab);
    }
    Services.obs.addObserver(tabCloser, "captive-portal-login-abort");
    Services.obs.addObserver(tabCloser, "captive-portal-login-success");
  },
};
