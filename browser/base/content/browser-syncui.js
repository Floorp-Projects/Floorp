# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

#ifdef MOZ_SERVICES_CLOUDSYNC
XPCOMUtils.defineLazyModuleGetter(this, "CloudSync",
                                  "resource://gre/modules/CloudSync.jsm");
#else
let CloudSync = null;
#endif

XPCOMUtils.defineLazyModuleGetter(this, "ReadingListScheduler",
                                  "resource:///modules/readinglist/Scheduler.jsm");

// gSyncUI handles updating the tools menu and displaying notifications.
let gSyncUI = {
  _obs: ["weave:service:sync:start",
         "weave:service:sync:finish",
         "weave:service:sync:error",
         "weave:service:quota:remaining",
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

         "readinglist:sync:start",
         "readinglist:sync:finish",
         "readinglist:sync:error",
  ],

  _unloaded: false,
  // The number of "active" syncs - while this is non-zero, our button will spin
  _numActiveSyncTasks: 0,

  init: function () {
    Cu.import("resource://services-common/stringbundle.js");

    // Proceed to set up the UI if Sync has already started up.
    // Otherwise we'll do it when Sync is firing up.
    let xps = Components.classes["@mozilla.org/weave/service;1"]
                                .getService(Components.interfaces.nsISupports)
                                .wrappedJSObject;
    if (xps.ready) {
      this.initUI();
      return;
    }

    Services.obs.addObserver(this, "weave:service:ready", true);

    // Remove the observer if the window is closed before the observer
    // was triggered.
    window.addEventListener("unload", function onUnload() {
      gSyncUI._unloaded = true;
      window.removeEventListener("unload", onUnload, false);
      Services.obs.removeObserver(gSyncUI, "weave:service:ready");

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

    if (gBrowser && Weave.Notifications.notifications.length) {
      this.initNotifications();
    }
    this.updateUI();
  },

  initNotifications: function SUI_initNotifications() {
    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let notificationbox = document.createElementNS(XULNS, "notificationbox");
    notificationbox.id = "sync-notifications";
    notificationbox.setAttribute("flex", "1");

    let bottombox = document.getElementById("browser-bottombox");
    bottombox.insertBefore(notificationbox, bottombox.firstChild);

    // Force a style flush to ensure that our binding is attached.
    notificationbox.clientTop;

    // notificationbox will listen to observers from now on.
    Services.obs.removeObserver(this, "weave:notification:added");
    let idx = this._obs.indexOf("weave:notification:added");
    if (idx >= 0) {
      this._obs.splice(idx, 1);
    }
  },

  _needsSetup() {
    // We want to treat "account needs verification" as "needs setup". So
    // "reach in" to Weave.Status._authManager to check whether we the signed-in
    // user is verified.
    // Referencing Weave.Status spins a nested event loop to initialize the
    // authManager, so this should always return a value directly.
    // This only applies to fxAccounts-based Sync.
    if (Weave.Status._authManager._signedInUser !== undefined) {
      // So we are using Firefox accounts - in this world, checking Sync isn't
      // enough as reading list may be configured but not Sync.
      // We consider ourselves setup if we have a verified user.
      // XXX - later we should consider checking preferences to ensure at least
      // one engine is enabled?
      return !Weave.Status._authManager._signedInUser ||
             !Weave.Status._authManager._signedInUser.verified;
    }

    // So we are using legacy sync, and reading-list isn't supported for such
    // users, so check sync itself.
    let firstSync = "";
    try {
      firstSync = Services.prefs.getCharPref("services.sync.firstSync");
    } catch (e) { }

    return Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED ||
           firstSync == "notReady";
  },

  _loginFailed: function () {
    this.log.debug("_loginFailed has sync state=${sync}, readinglist state=${rl}",
                   { sync: Weave.Status.login, rl: ReadingListScheduler.state});
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED ||
           ReadingListScheduler.state == ReadingListScheduler.STATE_ERROR_AUTHENTICATION;
  },

  updateUI: function SUI_updateUI() {
    let needsSetup = this._needsSetup();
    let loginFailed = this._loginFailed();

    // Start off with a clean slate
    document.getElementById("sync-reauth-state").hidden = true;
    document.getElementById("sync-setup-state").hidden = true;
    document.getElementById("sync-syncnow-state").hidden = true;

    if (CloudSync && CloudSync.ready && CloudSync().adapters.count) {
      document.getElementById("sync-syncnow-state").hidden = false;
    } else if (loginFailed) {
      document.getElementById("sync-reauth-state").hidden = false;
      this.showLoginError();
    } else if (needsSetup) {
      document.getElementById("sync-setup-state").hidden = false;
    } else {
      document.getElementById("sync-syncnow-state").hidden = false;
    }

    if (!gBrowser)
      return;

    let syncButton = document.getElementById("sync-button");
    let statusButton = document.getElementById("PanelUI-fxa-icon");
    if (needsSetup) {
      if (syncButton) {
        syncButton.removeAttribute("tooltiptext");
      }
      if (statusButton) {
        statusButton.removeAttribute("tooltiptext");
      }
    }

    this._updateLastSyncTime();
  },


  // Functions called by observers
  onActivityStart() {
    if (!gBrowser)
      return;

    this.log.debug("onActivityStart with numActive", this._numActiveSyncTasks);
    if (++this._numActiveSyncTasks == 1) {
      let button = document.getElementById("sync-button");
      if (button) {
        button.setAttribute("status", "active");
      }
      let container = document.getElementById("PanelUI-footer-fxa");
      if (container) {
        container.setAttribute("syncstatus", "active");
      }
    }
  },

  onActivityStop() {
    if (!gBrowser)
      return;
    this.log.debug("onActivityStop with numActive", this._numActiveSyncTasks);
    if (--this._numActiveSyncTasks) {
      if (this._numActiveSyncTasks < 0) {
        // This isn't particularly useful (it seems more likely we'll set a
        // "start" without a "stop" meaning it forever remains > 0) but it
        // might offer some value...
        this.log.error("mismatched onActivityStart/Stop calls",
                       new Error("active=" + this._numActiveSyncTasks));
      }
      return; // active tasks are still ongoing...
    }

    let syncButton = document.getElementById("sync-button");
    if (syncButton) {
      syncButton.removeAttribute("status");
    }
    let fxaContainer = document.getElementById("PanelUI-footer-fxa");
    if (fxaContainer) {
      fxaContainer.removeAttribute("syncstatus");
    }
  },

  onLoginFinish: function SUI_onLoginFinish() {
    // Clear out any login failure notifications
    let title = this._stringBundle.GetStringFromName("error.login.title");
    this.clearError(title);
  },

  onSetupComplete: function SUI_onSetupComplete() {
    this.onLoginFinish();
  },

  onLoginError: function SUI_onLoginError() {
    this.log.debug("onLoginError: login=${login}, sync=${sync}", Weave.Status);
    // Note: This is used for *both* Sync and ReadingList login errors.
    // if login fails, any other notifications are essentially moot
    Weave.Notifications.removeAll();

    // if we haven't set up the client, don't show errors
    if (this._needsSetup()) {
      this.updateUI();
      return;
    }
    // if we are still waiting for the identity manager to initialize, or it's
    // a network/server error, don't show errors.  If it weren't for the legacy
    // provider we could just check LOGIN_FAILED_LOGIN_REJECTED, but the legacy
    // provider has states like LOGIN_FAILED_INVALID_PASSPHRASE which we
    // probably do want to surface.
    if (Weave.Status.login == Weave.LOGIN_FAILED_NOT_READY ||
        Weave.Status.login == Weave.LOGIN_FAILED_NETWORK_ERROR ||
        Weave.Status.login == Weave.LOGIN_FAILED_SERVER_ERROR) {
      this.updateUI();
      return;
    }
    this.showLoginError();
    this.updateUI();
  },

  showLoginError() {
    // Note: This is used for *both* Sync and ReadingList login errors.
    let title = this._stringBundle.GetStringFromName("error.login.title");

    let description;
    if (Weave.Status.sync == Weave.PROLONGED_SYNC_FAILURE ||
        this.isProlongedReadingListError()) {
      this.log.debug("showLoginError has a prolonged login error");
      // Convert to days
      let lastSync =
        Services.prefs.getIntPref("services.sync.errorhandler.networkFailureReportTimeout") / 86400;
      description =
        this._stringBundle.formatStringFromName("error.sync.prolonged_failure", [lastSync], 1);
    } else {
      let reason = Weave.Utils.getErrorString(Weave.Status.login);
      description =
        this._stringBundle.formatStringFromName("error.sync.description", [reason], 1);
      this.log.debug("showLoginError has a non-prolonged error", reason);
    }

    let buttons = [];
    buttons.push(new Weave.NotificationButton(
      this._stringBundle.GetStringFromName("error.login.prefs.label"),
      this._stringBundle.GetStringFromName("error.login.prefs.accesskey"),
      function() { gSyncUI.openPrefs(); return true; }
    ));

    let notification = new Weave.Notification(title, description, null,
                                              Weave.Notifications.PRIORITY_WARNING, buttons);
    Weave.Notifications.replaceTitle(notification);
  },

  onLogout: function SUI_onLogout() {
    this.updateUI();
  },

  onStartOver: function SUI_onStartOver() {
    this.clearError();
  },

  onQuotaNotice: function onQuotaNotice(subject, data) {
    let title = this._stringBundle.GetStringFromName("warning.sync.quota.label");
    let description = this._stringBundle.GetStringFromName("warning.sync.quota.description");
    let buttons = [];
    buttons.push(new Weave.NotificationButton(
      this._stringBundle.GetStringFromName("error.sync.viewQuotaButton.label"),
      this._stringBundle.GetStringFromName("error.sync.viewQuotaButton.accesskey"),
      function() { gSyncUI.openQuotaDialog(); return true; }
    ));

    let notification = new Weave.Notification(
      title, description, null, Weave.Notifications.PRIORITY_WARNING, buttons);
    Weave.Notifications.replaceTitle(notification);
  },

  _getAppName: function () {
    let brand = new StringBundle("chrome://branding/locale/brand.properties");
    return brand.get("brandShortName");
  },

  openServerStatus: function () {
    let statusURL = Services.prefs.getCharPref("services.sync.statusURL");
    window.openUILinkIn(statusURL, "tab");
  },

  // Commands
  doSync: function SUI_doSync() {
    let needsSetup = this._needsSetup();

    if (!needsSetup) {
      setTimeout(function () Weave.Service.errorHandler.syncAndReportErrors(), 0);
    }

    Services.obs.notifyObservers(null, "cloudsync:user-sync", null);
    Services.obs.notifyObservers(null, "readinglist:user-sync", null);
  },

  handleToolbarButton: function SUI_handleStatusbarButton() {
    if (this._needsSetup())
      this.openSetup();
    else
      this.doSync();
  },

  //XXXzpao should be part of syncCommon.js - which we might want to make a module...
  //        To be fixed in a followup (bug 583366)

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
    let xps = Components.classes["@mozilla.org/weave/service;1"]
                                .getService(Components.interfaces.nsISupports)
                                .wrappedJSObject;
    if (xps.fxAccountsEnabled) {
      fxAccounts.getSignedInUser().then(userData => {
        if (userData) {
          this.openPrefs();
        } else {
          // If the user is also in an uitour, set the entrypoint to `uitour`
          if (UITour.tourBrowsersByWindow.get(window) &&
              UITour.tourBrowsersByWindow.get(window).has(gBrowser.selectedBrowser)) {
            entryPoint = "uitour";
          }
          switchToTabHavingURI("about:accounts?entrypoint=" + entryPoint, true, {
            replaceQueryString: true
          });
        }
      });
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

  openQuotaDialog: function SUI_openQuotaDialog() {
    let win = Services.wm.getMostRecentWindow("Sync:ViewQuota");
    if (win)
      win.focus();
    else
      Services.ww.activeWindow.openDialog(
        "chrome://browser/content/sync/quota.xul", "",
        "centerscreen,chrome,dialog,modal");
  },

  openPrefs: function SUI_openPrefs() {
    openPreferences("paneSync");
  },

  openSignInAgainPage: function (entryPoint = "syncbutton") {
    gFxAccounts.openSignInAgainPage(entryPoint);
  },

  // Helpers
  _updateLastSyncTime: function SUI__updateLastSyncTime() {
    if (!gBrowser)
      return;

    let syncButton = document.getElementById("sync-button");
    let statusButton = document.getElementById("PanelUI-fxa-icon");

    let lastSync;
    try {
      lastSync = new Date(Services.prefs.getCharPref("services.sync.lastSync"));
    }
    catch (e) { };
    // and reading-list time - we want whatever one is the most recent.
    try {
      let lastRLSync = new Date(Services.prefs.getCharPref("readinglist.scheduler.lastSync"));
      if (!lastSync || lastRLSync > lastSync) {
        lastSync = lastRLSync;
      }
    }
    catch (e) { };
    if (!lastSync || this._needsSetup()) {
      if (syncButton) {
        syncButton.removeAttribute("tooltiptext");
      }
      if (statusButton) {
        statusButton.removeAttribute("tooltiptext");
      }
      return;
    }

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSyncDateString = lastSync.toLocaleFormat("%a %H:%M");
    let lastSyncLabel =
      this._stringBundle.formatStringFromName("lastSync2.label", [lastSyncDateString], 1);

    if (syncButton) {
      syncButton.setAttribute("tooltiptext", lastSyncLabel);
    }
    if (statusButton) {
      statusButton.setAttribute("tooltiptext", lastSyncLabel);
    }
  },

  clearError: function SUI_clearError(errorString) {
    Weave.Notifications.removeAll(errorString);
    this.updateUI();
  },

  onSyncFinish: function SUI_onSyncFinish() {
    let title = this._stringBundle.GetStringFromName("error.sync.title");

    // Clear out sync failures on a successful sync
    this.clearError(title);
  },

  // Return true if the reading-list is in a "prolonged" error state. That
  // engine doesn't impose what that means, so calculate it here. For
  // consistency, we just use the sync prefs.
  isProlongedReadingListError() {
    // If the readinglist scheduler is disabled we don't treat it as prolonged.
    let enabled = false;
    try {
      enabled = Services.prefs.getBoolPref("readinglist.scheduler.enabled");
    } catch (_) {}
    if (!enabled) {
      return false;
    }
    let lastSync, threshold, prolonged;
    try {
      lastSync = new Date(Services.prefs.getCharPref("readinglist.scheduler.lastSync"));
      threshold = new Date(Date.now() - Services.prefs.getIntPref("services.sync.errorhandler.networkFailureReportTimeout") * 1000);
      prolonged = lastSync <= threshold;
    } catch (ex) {
      // no pref, assume not prolonged.
      prolonged = false;
    }
    this.log.debug("isProlongedReadingListError has last successful sync at ${lastSync}, threshold is ${threshold}, prolonged=${prolonged}",
                   {lastSync, threshold, prolonged});
    return prolonged;
  },

  onRLSyncError() {
    // Like onSyncError, but from the reading-list engine.
    // However, the current UX around Sync is that error notifications should
    // generally *not* be seen as they typically aren't actionable - so only
    // authentication errors (which require user action) and "prolonged" errors
    // (which technically aren't actionable, but user really should know anyway)
    // are shown.
    this.log.debug("onRLSyncError with readingList state", ReadingListScheduler.state);
    if (ReadingListScheduler.state == ReadingListScheduler.STATE_ERROR_AUTHENTICATION) {
      this.onLoginError();
      return;
    }
    // If it's not prolonged there's nothing to do.
    if (!this.isProlongedReadingListError()) {
      this.log.debug("onRLSyncError has a non-authentication, non-prolonged error, so not showing any error UI");
      return;
    }
    // So it's a prolonged error.
    // Unfortunate duplication from below...
    this.log.debug("onRLSyncError has a prolonged error");
    let title = this._stringBundle.GetStringFromName("error.sync.title");
    // XXX - this is somewhat wrong - we are reporting the threshold we consider
    // to be prolonged, not how long it actually has been. (ie, lastSync below
    // is effectively constant) - bit it too is copied from below.
    let lastSync =
      Services.prefs.getIntPref("services.sync.errorhandler.networkFailureReportTimeout") / 86400;
    let description =
      this._stringBundle.formatStringFromName("error.sync.prolonged_failure", [lastSync], 1);
    let priority = Weave.Notifications.PRIORITY_INFO;
    let buttons = [
      new Weave.NotificationButton(
        this._stringBundle.GetStringFromName("error.sync.tryAgainButton.label"),
        this._stringBundle.GetStringFromName("error.sync.tryAgainButton.accesskey"),
        function() { gSyncUI.doSync(); return true; }
      ),
    ];
    let notification =
      new Weave.Notification(title, description, null, priority, buttons);
    Weave.Notifications.replaceTitle(notification);

    this.updateUI();
  },

  onSyncError: function SUI_onSyncError() {
    this.log.debug("onSyncError: login=${login}, sync=${sync}", Weave.Status);
    let title = this._stringBundle.GetStringFromName("error.sync.title");

    if (Weave.Status.login != Weave.LOGIN_SUCCEEDED) {
      this.onLoginError();
      return;
    }

    let description;
    if (Weave.Status.sync == Weave.PROLONGED_SYNC_FAILURE) {
      // Convert to days
      let lastSync =
        Services.prefs.getIntPref("services.sync.errorhandler.networkFailureReportTimeout") / 86400;
      description =
        this._stringBundle.formatStringFromName("error.sync.prolonged_failure", [lastSync], 1);
    } else {
      let error = Weave.Utils.getErrorString(Weave.Status.sync);
      description =
        this._stringBundle.formatStringFromName("error.sync.description", [error], 1);
    }
    let priority = Weave.Notifications.PRIORITY_WARNING;
    let buttons = [];

    // Check if the client is outdated in some way (but note: we've never in the
    // past, and probably never will, bump the relevent version numbers, so
    // this is effectively dead code!)
    let outdated = Weave.Status.sync == Weave.VERSION_OUT_OF_DATE;
    for (let [engine, reason] in Iterator(Weave.Status.engines))
      outdated = outdated || reason == Weave.VERSION_OUT_OF_DATE;

    if (outdated) {
      description = this._stringBundle.GetStringFromName(
        "error.sync.needUpdate.description");
      buttons.push(new Weave.NotificationButton(
        this._stringBundle.GetStringFromName("error.sync.needUpdate.label"),
        this._stringBundle.GetStringFromName("error.sync.needUpdate.accesskey"),
        function() { window.openUILinkIn("https://services.mozilla.com/update/", "tab"); return true; }
      ));
    }
    else if (Weave.Status.sync == Weave.OVER_QUOTA) {
      description = this._stringBundle.GetStringFromName(
        "error.sync.quota.description");
      buttons.push(new Weave.NotificationButton(
        this._stringBundle.GetStringFromName(
          "error.sync.viewQuotaButton.label"),
        this._stringBundle.GetStringFromName(
          "error.sync.viewQuotaButton.accesskey"),
        function() { gSyncUI.openQuotaDialog(); return true; } )
      );
    }
    else if (Weave.Status.enforceBackoff) {
      priority = Weave.Notifications.PRIORITY_INFO;
      buttons.push(new Weave.NotificationButton(
        this._stringBundle.GetStringFromName("error.sync.serverStatusButton.label"),
        this._stringBundle.GetStringFromName("error.sync.serverStatusButton.accesskey"),
        function() { gSyncUI.openServerStatus(); return true; }
      ));
    }
    else {
      priority = Weave.Notifications.PRIORITY_INFO;
      buttons.push(new Weave.NotificationButton(
        this._stringBundle.GetStringFromName("error.sync.tryAgainButton.label"),
        this._stringBundle.GetStringFromName("error.sync.tryAgainButton.accesskey"),
        function() { gSyncUI.doSync(); return true; }
      ));
    }

    let notification =
      new Weave.Notification(title, description, null, priority, buttons);
    Weave.Notifications.replaceTitle(notification);

    this.updateUI();
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
      case "weave:service:login:start":
      case "readinglist:sync:start":
        this.onActivityStart();
        break;
      case "weave:service:sync:finish":
      case "weave:service:sync:error":
      case "weave:service:login:finish":
      case "weave:service:login:error":
      case "readinglist:sync:finish":
      case "readinglist:sync:error":
        this.onActivityStop();
        break;
    }
    // Now non-activity state (eg, enabled, errors, etc)
    // Note that sync uses the ":ui:" notifications for errors because sync.
    // ReadingList has no such concept (yet?; hopefully the :error is enough!)
    switch (topic) {
      case "weave:ui:sync:finish":
        this.onSyncFinish();
        break;
      case "weave:ui:sync:error":
        this.onSyncError();
        break;
      case "weave:service:quota:remaining":
        this.onQuotaNotice();
        break;
      case "weave:service:setup-complete":
        this.onSetupComplete();
        break;
      case "weave:service:login:finish":
        this.onLoginFinish();
        break;
      case "weave:ui:login:error":
        this.onLoginError();
        break;
      case "weave:service:logout:finish":
        this.onLogout();
        break;
      case "weave:service:start-over":
        this.onStartOver();
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
      case "weave:ui:clear-error":
        this.clearError();
        break;

      case "readinglist:sync:error":
        this.onRLSyncError();
        break;
      case "readinglist:sync:finish":
        this.clearError();
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
