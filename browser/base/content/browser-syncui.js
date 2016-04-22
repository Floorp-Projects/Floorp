/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

if (AppConstants.MOZ_SERVICES_CLOUDSYNC) {
  XPCOMUtils.defineLazyModuleGetter(this, "CloudSync",
                                    "resource://gre/modules/CloudSync.jsm");
}

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

const MIN_STATUS_ANIMATION_DURATION = 1600;

// gSyncUI handles updating the tools menu and displaying notifications.
var gSyncUI = {
  _obs: ["weave:service:sync:start",
         "weave:service:sync:finish",
         "weave:service:sync:error",
         "weave:service:setup-complete",
         "weave:service:login:start",
         "weave:service:login:finish",
         "weave:service:login:error",
         "weave:service:logout:finish",
         "weave:service:start-over",
         "weave:service:start-over:finish",
         "weave:ui:login:error",
         "weave:ui:sync:error",
         "weave:ui:sync:finish",
         "weave:ui:clear-error",
         "weave:engine:sync:finish"
  ],

  _unloaded: false,
  // The last sync start time. Used to calculate the leftover animation time
  // once syncing completes (bug 1239042).
  _syncStartTime: 0,
  _syncAnimationTimer: 0,

  init: function () {
    Cu.import("resource://services-common/stringbundle.js");

    // Proceed to set up the UI if Sync has already started up.
    // Otherwise we'll do it when Sync is firing up.
    if (this.weaveService.ready) {
      this.initUI();
      return;
    }

    // Sync isn't ready yet, but we can still update the UI with an initial
    // state - we haven't called initUI() yet, but that's OK - that's more
    // about observers for state changes, and will be called once Sync is
    // ready to start sending notifications.
    this.updateUI();

    Services.obs.addObserver(this, "weave:service:ready", true);
    Services.obs.addObserver(this, "quit-application", true);

    // Remove the observer if the window is closed before the observer
    // was triggered.
    window.addEventListener("unload", function onUnload() {
      gSyncUI._unloaded = true;
      window.removeEventListener("unload", onUnload, false);
      Services.obs.removeObserver(gSyncUI, "weave:service:ready");
      Services.obs.removeObserver(gSyncUI, "quit-application");

      if (Weave.Status.ready) {
        gSyncUI._obs.forEach(function(topic) {
          Services.obs.removeObserver(gSyncUI, topic);
        });
      }
    }, false);
  },

  initUI: function SUI_initUI() {
    // If this is a browser window?
    if (gBrowser) {
      this._obs.push("weave:notification:added");
    }

    this._obs.forEach(function(topic) {
      Services.obs.addObserver(this, topic, true);
    }, this);

    // initial label for the sync buttons.
    let broadcaster = document.getElementById("sync-status");
    broadcaster.setAttribute("label", this._stringBundle.GetStringFromName("syncnow.label"));

    this.maybeMoveSyncedTabsButton();

    this.updateUI();
  },


  // Returns a promise that resolves with true if Sync needs to be configured,
  // false otherwise.
  _needsSetup() {
    // If Sync is configured for FxAccounts then we do that promise-dance.
    if (this.weaveService.fxAccountsEnabled) {
      return fxAccounts.getSignedInUser().then(user => {
        // We want to treat "account needs verification" as "needs setup".
        return !(user && user.verified);
      });
    }
    // We are using legacy sync - check that.
    let firstSync = "";
    try {
      firstSync = Services.prefs.getCharPref("services.sync.firstSync");
    } catch (e) { }

    return Promise.resolve(Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED ||
                           firstSync == "notReady");
  },

  // Returns a promise that resolves with true if the user currently signed in
  // to Sync needs to be verified, false otherwise.
  _needsVerification() {
    // For callers who care about the distinction between "needs setup" and
    // "needs verification"
    if (this.weaveService.fxAccountsEnabled) {
      return fxAccounts.getSignedInUser().then(user => {
        // If there is no user, they can't be in a "needs verification" state.
        if (!user) {
          return false;
        }
        return !user.verified;
      });
    }

    // Otherwise we are configured for legacy Sync, which has no verification
    // concept.
    return Promise.resolve(false);
  },

  // Note that we don't show login errors in a notification bar here, but do
  // still need to track a login-failed state so the "Tools" menu updates
  // with the correct state.
  _loginFailed: function () {
    // If Sync isn't already ready, we don't want to force it to initialize
    // by referencing Weave.Status - and it isn't going to be accurate before
    // Sync is ready anyway.
    if (!this.weaveService.ready) {
      this.log.debug("_loginFailed has sync not ready, so returning false");
      return false;
    }
    this.log.debug("_loginFailed has sync state=${sync}",
                   { sync: Weave.Status.login});
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  // Kick off an update of the UI - does *not* return a promise.
  updateUI() {
    this._promiseUpdateUI().catch(err => {
      this.log.error("updateUI failed", err);
    })
  },

  // Updates the UI - returns a promise.
  _promiseUpdateUI() {
    return this._needsSetup().then(needsSetup => {
      if (!gBrowser)
        return Promise.resolve();

      let loginFailed = this._loginFailed();

      // Start off with a clean slate
      document.getElementById("sync-reauth-state").hidden = true;
      document.getElementById("sync-setup-state").hidden = true;
      document.getElementById("sync-syncnow-state").hidden = true;

      if (CloudSync && CloudSync.ready && CloudSync().adapters.count) {
        document.getElementById("sync-syncnow-state").hidden = false;
      } else if (loginFailed) {
        // unhiding this element makes the menubar show the login failure state.
        document.getElementById("sync-reauth-state").hidden = false;
      } else if (needsSetup) {
        document.getElementById("sync-setup-state").hidden = false;
      } else {
        document.getElementById("sync-syncnow-state").hidden = false;
      }

      return this._updateSyncButtonsTooltip();
    });
  },

  // Functions called by observers
  onActivityStart() {
    if (!gBrowser)
      return;

    this.log.debug("onActivityStart");

    clearTimeout(this._syncAnimationTimer);
    this._syncStartTime = Date.now();

    let broadcaster = document.getElementById("sync-status");
    broadcaster.setAttribute("syncstatus", "active");
    broadcaster.setAttribute("label", this._stringBundle.GetStringFromName("syncing2.label"));
    broadcaster.setAttribute("disabled", "true");

    this.updateUI();
  },

  _updateSyncStatus() {
    if (!gBrowser)
      return;
    let broadcaster = document.getElementById("sync-status");
    broadcaster.removeAttribute("syncstatus");
    broadcaster.removeAttribute("disabled");
    broadcaster.setAttribute("label", this._stringBundle.GetStringFromName("syncnow.label"));
    this.updateUI();
  },

  onActivityStop() {
    if (!gBrowser)
      return;
    this.log.debug("onActivityStop");

    let now = Date.now();
    let syncDuration = now - this._syncStartTime;

    if (syncDuration < MIN_STATUS_ANIMATION_DURATION) {
      let animationTime = MIN_STATUS_ANIMATION_DURATION - syncDuration;
      clearTimeout(this._syncAnimationTimer);
      this._syncAnimationTimer = setTimeout(() => this._updateSyncStatus(), animationTime);
    } else {
      this._updateSyncStatus();
    }
  },

  onLoginError: function SUI_onLoginError() {
    this.log.debug("onLoginError: login=${login}, sync=${sync}", Weave.Status);

    // We don't show any login errors here; browser-fxaccounts shows them in
    // the hamburger menu.
    this.updateUI();
  },

  onLogout: function SUI_onLogout() {
    this.updateUI();
  },

  _getAppName: function () {
    let brand = new StringBundle("chrome://branding/locale/brand.properties");
    return brand.get("brandShortName");
  },

  // Commands
  // doSync forces a sync - it *does not* return a promise as it is called
  // via the various UI components.
  doSync() {
    this._needsSetup().then(needsSetup => {
      if (!needsSetup) {
        setTimeout(() => Weave.Service.errorHandler.syncAndReportErrors(), 0);
      }
      Services.obs.notifyObservers(null, "cloudsync:user-sync", null);
    }).catch(err => {
      this.log.error("Failed to force a sync", err);
    });
  },

  // Handle clicking the toolbar button - which either opens the Sync setup
  // pages or forces a sync now. Does *not* return a promise as it is called
  // via the UI.
  handleToolbarButton() {
    this._needsSetup().then(needsSetup => {
      if (needsSetup || this._loginFailed()) {
        this.openSetup();
      } else {
        this.doSync();
      }
    }).catch(err => {
      this.log.error("Failed to handle toolbar button command", err);
    });
  },

  /**
   * Invoke the Sync setup wizard.
   *
   * @param wizardType
   *        Indicates type of wizard to launch:
   *          null    -- regular set up wizard
   *          "pair"  -- pair a device first
   *          "reset" -- reset sync
   * @param entryPoint
   *        Indicates the entrypoint from where this method was called.
   */

  openSetup: function SUI_openSetup(wizardType, entryPoint = "syncbutton") {
    if (this.weaveService.fxAccountsEnabled) {
      // If the user is also in an uitour, set the entrypoint to `uitour`
      if (UITour.tourBrowsersByWindow.get(window) &&
          UITour.tourBrowsersByWindow.get(window).has(gBrowser.selectedBrowser)) {
        entryPoint = "uitour";
      }
      this.openPrefs(entryPoint);
    } else {
      let win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
      if (win)
        win.focus();
      else {
        window.openDialog("chrome://browser/content/sync/setup.xul",
                          "weaveSetup", "centerscreen,chrome,resizable=no",
                          wizardType);
      }
    }
  },

  // Open the legacy-sync device pairing UI. Note used for FxA Sync.
  openAddDevice: function () {
    if (!Weave.Utils.ensureMPUnlocked())
      return;

    let win = Services.wm.getMostRecentWindow("Sync:AddDevice");
    if (win)
      win.focus();
    else
      window.openDialog("chrome://browser/content/sync/addDevice.xul",
                        "syncAddDevice", "centerscreen,chrome,resizable=no");
  },

  openPrefs: function (entryPoint) {
    openPreferences("paneSync", { urlParams: { entrypoint: entryPoint } });
  },

  openSignInAgainPage: function (entryPoint = "syncbutton") {
    gFxAccounts.openSignInAgainPage(entryPoint);
  },

  openSyncedTabsPanel() {
    let placement = CustomizableUI.getPlacementOfWidget("sync-button");
    let area = placement ? placement.area : CustomizableUI.AREA_NAVBAR;
    let anchor = document.getElementById("sync-button") ||
                 document.getElementById("PanelUI-menu-button");
    if (area == CustomizableUI.AREA_PANEL) {
      // The button is in the panel, so we need to show the panel UI, then our
      // subview.
      PanelUI.show().then(() => {
        PanelUI.showSubView("PanelUI-remotetabs", anchor, area);
      }).catch(Cu.reportError);
    } else {
      // It is placed somewhere else - just try and show it.
      PanelUI.showSubView("PanelUI-remotetabs", anchor, area);
    }
  },

  /* After Sync is initialized we perform a once-only check for the sync
     button being in "customize purgatory" and if so, move it to the panel.
     This is done primarily for profiles created before SyncedTabs landed,
     where the button defaulted to being in that purgatory.
     We use a preference to ensure we only do it once, so people can still
     customize it away and have it stick.
  */
  maybeMoveSyncedTabsButton() {
    const prefName = "browser.migrated-sync-button";
    let migrated = false;
    try {
      migrated = Services.prefs.getBoolPref(prefName);
    } catch (_) {}
    if (migrated) {
      return;
    }
    if (!CustomizableUI.getPlacementOfWidget("sync-button")) {
      CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);
    }
    Services.prefs.setBoolPref(prefName, true);
  },

  /* Update the tooltip for the sync-status broadcaster (which will update the
     Sync Toolbar button and the Sync spinner in the FxA hamburger area.)
     If Sync is configured, the tooltip is when the last sync occurred,
     otherwise the tooltip reflects the fact that Sync needs to be
     (re-)configured.
  */
  _updateSyncButtonsTooltip: Task.async(function* () {
    if (!gBrowser)
      return;

    let email;
    try {
      email = Services.prefs.getCharPref("services.sync.username");
    } catch (ex) {}

    let needsSetup = yield this._needsSetup();
    let needsVerification = yield this._needsVerification();
    let loginFailed = this._loginFailed();
    // This is a little messy as the Sync buttons are 1/2 Sync related and
    // 1/2 FxA related - so for some strings we use Sync strings, but for
    // others we reach into gFxAccounts for strings.
    let tooltiptext;
    if (needsVerification) {
      // "needs verification"
      tooltiptext = gFxAccounts.strings.formatStringFromName("verifyDescription", [email], 1);
    } else if (needsSetup) {
      // "needs setup".
      tooltiptext = this._stringBundle.GetStringFromName("signInToSync.description");
    } else if (loginFailed) {
      // "need to reconnect/re-enter your password"
      tooltiptext = gFxAccounts.strings.formatStringFromName("reconnectDescription", [email], 1);
    } else {
      // Sync appears configured - format the "last synced at" time.
      try {
        let lastSync = new Date(Services.prefs.getCharPref("services.sync.lastSync"));
        // Show the day-of-week and time (HH:MM) of last sync
        let lastSyncDateString = lastSync.toLocaleDateString(undefined,
          {weekday: 'long', hour: 'numeric', minute: 'numeric'});
        tooltiptext = this._stringBundle.formatStringFromName("lastSync2.label", [lastSyncDateString], 1);
      }
      catch (e) {
        // pref doesn't exist (which will be the case until we've seen the
        // first successful sync) or is invalid (which should be impossible!)
        // Just leave tooltiptext as the empty string in these cases, which
        // will cause the tooltip to be removed below.
      }
    }

    // We've done all our promise-y work and ready to update the UI - make
    // sure it hasn't been torn down since we started.
    if (!gBrowser)
      return;

    let broadcaster = document.getElementById("sync-status");
    if (broadcaster) {
      if (tooltiptext) {
        broadcaster.setAttribute("tooltiptext", tooltiptext);
      } else {
        broadcaster.removeAttribute("tooltiptext");
      }
    }
  }),

  onSyncFinish: function SUI_onSyncFinish() {
    let title = this._stringBundle.GetStringFromName("error.sync.title");
  },

  onClientsSynced: function() {
    let broadcaster = document.getElementById("sync-syncnow-state");
    if (broadcaster) {
      if (Weave.Service.clientsEngine.stats.numClients > 1) {
        broadcaster.setAttribute("devices-status", "multi");
      } else {
        broadcaster.setAttribute("devices-status", "single");
      }
    }
  },

  observe: function SUI_observe(subject, topic, data) {
    this.log.debug("observed", topic);
    if (this._unloaded) {
      Cu.reportError("SyncUI observer called after unload: " + topic);
      return;
    }

    // Unwrap, just like Svc.Obs, but without pulling in that dependency.
    if (subject && typeof subject == "object" &&
        ("wrappedJSObject" in subject) &&
        ("observersModuleSubjectWrapper" in subject.wrappedJSObject)) {
      subject = subject.wrappedJSObject.object;
    }

    // First handle "activity" only.
    switch (topic) {
      case "weave:service:sync:start":
        this.onActivityStart();
        break;
      case "weave:service:sync:finish":
      case "weave:service:sync:error":
        this.onActivityStop();
        break;
    }
    // Now non-activity state (eg, enabled, errors, etc)
    // Note that sync uses the ":ui:" notifications for errors because sync.
    switch (topic) {
      case "weave:ui:sync:finish":
        this.onSyncFinish();
        break;
      case "weave:ui:sync:error":
      case "weave:service:setup-complete":
      case "weave:service:login:finish":
      case "weave:service:login:start":
      case "weave:service:start-over":
        this.updateUI();
        break;
      case "weave:ui:login:error":
      case "weave:service:login:error":
        this.onLoginError();
        break;
      case "weave:service:logout:finish":
        this.onLogout();
        break;
      case "weave:service:start-over:finish":
        this.updateUI();
        break;
      case "weave:service:ready":
        this.initUI();
        break;
      case "weave:notification:added":
        this.initNotifications();
        break;
      case "weave:engine:sync:finish":
        if (data != "clients") {
          return;
        }
        this.onClientsSynced();
        break;
      case "quit-application":
        // Stop the animation timer on shutdown, since we can't update the UI
        // after this.
        clearTimeout(this._syncAnimationTimer);
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};

XPCOMUtils.defineLazyGetter(gSyncUI, "_stringBundle", function() {
  //XXXzpao these strings should probably be moved from /services to /browser... (bug 583381)
  //        but for now just make it work
  return Cc["@mozilla.org/intl/stringbundle;1"].
         getService(Ci.nsIStringBundleService).
         createBundle("chrome://weave/locale/services/sync.properties");
});

XPCOMUtils.defineLazyGetter(gSyncUI, "log", function() {
  return Log.repository.getLogger("browserwindow.syncui");
});

XPCOMUtils.defineLazyGetter(gSyncUI, "weaveService", function() {
  return Components.classes["@mozilla.org/weave/service;1"]
                   .getService(Components.interfaces.nsISupports)
                   .wrappedJSObject;
});
