# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Bookmarks Sync.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Dan Mills <thunder@mozilla.com>
#  Chris Beard <cbeard@mozilla.com>
#  Dan Mosedale <dmose@mozilla.org>
#  Paul O’Shannessy <paul@oshannessy.com>
#  Philipp von Weitershausen <philipp@weitershausen.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

// gSyncUI handles updating the tools menu
let gSyncUI = {
  init: function SUI_init() {
    // this will be the first notification fired during init
    // we can set up everything else later
    Services.obs.addObserver(this, "weave:service:ready", true);

    // Remove the observer if the window is closed before the observer
    // was triggered.
    window.addEventListener("unload", function() {
      window.removeEventListener("unload", arguments.callee, false);
      Services.obs.removeObserver(gSyncUI, "weave:service:ready");
    }, false);
  },
  initUI: function SUI_initUI() {
    let obs = ["weave:service:sync:start",
               "weave:service:sync:finish",
               "weave:service:sync:error",
               "weave:service:sync:delayed",
               "weave:service:quota:remaining",
               "weave:service:setup-complete",
               "weave:service:login:start",
               "weave:service:login:finish",
               "weave:service:login:error",
               "weave:service:logout:finish",
               "weave:service:start-over"];

    // If this is a browser window?
    if (gBrowser) {
      obs.push("weave:notification:added");
    }

    let self = this;
    obs.forEach(function(topic) {
      Services.obs.addObserver(self, topic, true);
    });

    // Find the alltabs-popup, only if there is a gBrowser
    if (gBrowser) {
      let popup = document.getElementById("alltabs-popup");
      let self = this;
      popup.addEventListener("popupshowing", function() {
        self.alltabsPopupShowing();
      }, true);

      if (Weave.Notifications.notifications.length)
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
    let statusbar = document.getElementById("status-bar");
    bottombox.insertBefore(notificationbox, statusbar);

    // Force a style flush to ensure that our binding is attached.
    notificationbox.clientTop;

    // notificationbox will listen to observers from now on.
    Services.obs.removeObserver(this, "weave:notification:added");
  },

  _wasDelayed: false,

  _needsSetup: function SUI__needsSetup() {
    let firstSync = "";
    try {
      firstSync = Services.prefs.getCharPref("services.sync.firstSync");
    } catch (e) { }
    return Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED ||
           firstSync == "notReady";
  },

  _isLoggedIn: function() {
    if (this._needsSetup())
      return false;
    return Weave.Service.isLoggedIn;
  },

  updateUI: function SUI_updateUI() {
    let needsSetup = this._needsSetup();
    document.getElementById("sync-setup-state").hidden = !needsSetup;
    document.getElementById("sync-syncnow-state").hidden = needsSetup;

    if (!gBrowser)
      return;

    let button = document.getElementById("sync-button");
    if (!button)
      return;

    button.removeAttribute("status");
    this._updateLastSyncTime();
    if (needsSetup)
      button.removeAttribute("tooltiptext");
  },

  alltabsPopupShowing: function(event) {
    // Should we show the menu item?
    if (!Weave.Service.isLoggedIn || !Weave.Engines.get("tabs").enabled)
      return;

    let label = this._stringBundle.GetStringFromName("tabs.fromOtherComputers.label");

    let popup = document.getElementById("alltabs-popup");
    let menuitem = document.createElement("menuitem");
    menuitem.setAttribute("id", "sync-tabs-menuitem");
    menuitem.setAttribute("label", label);
    menuitem.setAttribute("class", "alltabs-item");
    menuitem.setAttribute("oncommand", "BrowserOpenSyncTabs();");

    let sep = document.createElement("menuseparator");
    sep.setAttribute("id", "sync-tabs-sep");

    // Fake the tab object on the menu entries, so that we don't have to worry
    // about removing them ourselves. They will just get cleaned up by popup
    // binding.
    menuitem.tab = { "linkedBrowser": { "currentURI": { "spec": label } } };
    sep.tab = { "linkedBrowser": { "currentURI": { "spec": " " } } };

    popup.insertBefore(sep, popup.firstChild);
    popup.insertBefore(menuitem, sep);
  },


  // Functions called by observers
  onActivityStart: function SUI_onActivityStart() {
    if (!gBrowser)
      return;

    let button = document.getElementById("sync-button");
    if (!button)
      return;

    button.setAttribute("status", "active");
  },

  onSyncFinish: function SUI_onSyncFinish() {
    this._onSyncEnd(true);
  },

  onSyncError: function SUI_onSyncError() {
    this._onSyncEnd(false);
  },

  onSyncDelay: function SUI_onSyncDelay() {
    // basically, we want to just inform users that stuff is going to take a while
    let title = this._stringBundle.GetStringFromName("error.sync.no_node_found.title");
    let description = this._stringBundle.GetStringFromName("error.sync.no_node_found");
    let notification = new Weave.Notification(title, description, null, Weave.Notifications.PRIORITY_INFO);
    Weave.Notifications.replaceTitle(notification);
    this._wasDelayed = true;
  },

  onLoginFinish: function SUI_onLoginFinish() {
    // Clear out any login failure notifications
    let title = this._stringBundle.GetStringFromName("error.login.title");
    Weave.Notifications.removeAll(title);

    this.updateUI();
    this._updateLastSyncTime();
  },

  onLoginError: function SUI_onLoginError() {
    // if login fails, any other notifications are essentially moot
    Weave.Notifications.removeAll();

    // if we haven't set up the client, don't show errors
    if (this._needsSetup()) {
      this.updateUI();
      return;
    }

    let title = this._stringBundle.GetStringFromName("error.login.title");
    let reason = Weave.Utils.getErrorString(Weave.Status.login);
    let description =
      this._stringBundle.formatStringFromName("error.login.description", [reason], 1);
    let buttons = [];
    buttons.push(new Weave.NotificationButton(
      this._stringBundle.GetStringFromName("error.login.prefs.label"),
      this._stringBundle.GetStringFromName("error.login.prefs.accesskey"),
      function() { gSyncUI.openPrefs(); return true; }
    ));

    let notification = new Weave.Notification(title, description, null,
                                              Weave.Notifications.PRIORITY_WARNING, buttons);
    Weave.Notifications.replaceTitle(notification);
    this.updateUI();
  },

  onLogout: function SUI_onLogout() {
    this.updateUI();
  },

  onStartOver: function SUI_onStartOver() {
    this.updateUI();
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

  // Commands
  doLogin: function SUI_doLogin() {
    Weave.Service.login();
  },

  doLogout: function SUI_doLogout() {
    Weave.Service.logout();
  },

  doSync: function SUI_doSync() {
    if (Weave.Service.isLoggedIn || Weave.Service.login())
      Weave.Service.sync();
  },

  handleToolbarButton: function SUI_handleStatusbarButton() {
    if (this._needsSetup())
      this.openSetup();
    else
      this.doSync();
  },

  //XXXzpao should be part of syncCommon.js - which we might want to make a module...
  //        To be fixed in a followup (bug 583366)
  openSetup: function SUI_openSetup() {
    let win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
    if (win)
      win.focus();
    else {
      window.openDialog("chrome://browser/content/syncSetup.xul",
                        "weaveSetup", "centerscreen,chrome,resizable=no");
    }
  },

  openQuotaDialog: function SUI_openQuotaDialog() {
    let win = Services.wm.getMostRecentWindow("Sync:ViewQuota");
    if (win)
      win.focus();
    else 
      Services.ww.activeWindow.openDialog(
        "chrome://browser/content/syncQuota.xul", "",
        "centerscreen,chrome,dialog,modal");
  },

  openPrefs: function SUI_openPrefs() {
    openPreferences("paneSync");
  },


  // Helpers
  _updateLastSyncTime: function SUI__updateLastSyncTime() {
    if (!gBrowser)
      return;

    let syncButton = document.getElementById("sync-button");
    if (!syncButton)
      return;

    let lastSync;
    try {
      lastSync = Services.prefs.getCharPref("services.sync.lastSync");
    }
    catch (e) { };
    if (!lastSync || this._needsSetup()) {
      syncButton.removeAttribute("tooltiptext");
      return;
    }

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSyncDate = new Date(lastSync).toLocaleFormat("%a %H:%M");
    let lastSyncLabel =
      this._stringBundle.formatStringFromName("lastSync.label", [lastSyncDate], 1);

    syncButton.setAttribute("tooltiptext", lastSyncLabel);
  },

  _onSyncEnd: function SUI__onSyncEnd(success) {
    let title = this._stringBundle.GetStringFromName("error.sync.title");
    if (!success) {
      if (Weave.Status.login != Weave.LOGIN_SUCCEEDED) {
        this.onLoginError();
        return;
      }
      let error = Weave.Utils.getErrorString(Weave.Status.sync);
      let description =
        this._stringBundle.formatStringFromName("error.sync.description", [error], 1);

      let priority = Weave.Notifications.PRIORITY_WARNING;
      let buttons = [];

      if (Weave.Status.sync == Weave.OVER_QUOTA) {
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
      else if (!Weave.Status.enforceBackoff) {
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
    }
    else {
      // Clear out sync failures on a successful sync
      Weave.Notifications.removeAll(title);
    }

    if (this._wasDelayed && Weave.Status.sync != Weave.NO_SYNC_NODE_FOUND) {
      title = this._stringBundle.GetStringFromName("error.sync.no_node_found.title");
      Weave.Notifications.removeAll(title);
      this._wasDelayed = false;
    }

    this.updateUI();
    this._updateLastSyncTime();
  },
  
  observe: function SUI_observe(subject, topic, data) {
    switch (topic) {
      case "weave:service:sync:start":
        this.onActivityStart();
        break;
      case "weave:service:sync:finish":
        this.onSyncFinish();
        break;
      case "weave:service:sync:error":
        this.onSyncError();
        break;
      case "weave:service:sync:delayed":
        this.onSyncDelay();
        break;
      case "weave:service:quota:remaining":
        this.onQuotaNotice();
        break;
      case "weave:service:setup-complete":
        this.onLoginFinish();
        break;
      case "weave:service:login:start":
        this.onActivityStart();
        break;
      case "weave:service:login:finish":
        this.onLoginFinish();
        break;
      case "weave:service:login:error":
        this.onLoginError();
        break;
      case "weave:service:logout:finish":
        this.onLogout();
        break;
      case "weave:service:start-over":
        this.onStartOver();
        break;
      case "weave:service:ready":
        this.initUI();
        break;
      case "weave:notification:added":
        this.initNotifications();
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

