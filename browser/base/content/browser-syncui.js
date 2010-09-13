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
               "weave:service:setup-complete",
               "weave:service:login:start",
               "weave:service:login:finish",
               "weave:service:login:error",
               "weave:service:logout:finish",
               "weave:service:start-over"];

    // If this is a browser window?
    if (gBrowser) {
      obs.push("weave:notification:added", "weave:notification:removed");
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
    }
    this.updateUI();
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
    document.getElementById("sync-setup").hidden = !needsSetup;
    document.getElementById("sync-menu").hidden = needsSetup;

    if (gBrowser) {
      let showLabel = !this._isLoggedIn() && !needsSetup;
      let button = document.getElementById("sync-status-button");
      button.setAttribute("class", showLabel ? "statusbarpanel-iconic-text"
                                             : "statusbarpanel-iconic");
      button.image = "chrome://browser/skin/sync-16.png";

      if (!this._isLoggedIn()) {
        //XXXzpao When we move the string bundle, we can add more and make this
        //        say "needs setup" or something similar. (bug 583381)
        button.removeAttribute("tooltiptext");
      }
    }
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
    // binding. This also makes sure the statusbar updates with the URL.
    menuitem.tab = { "linkedBrowser": { "currentURI": { "spec": label } } };
    sep.tab = { "linkedBrowser": { "currentURI": { "spec": " " } } };

    popup.insertBefore(sep, popup.firstChild);
    popup.insertBefore(menuitem, sep);
  },


  // Functions called by observers
  onActivityStart: function SUI_onActivityStart() {
    //XXXzpao Followup: Do this with a class. (bug 583384)
    if (gBrowser)
      document.getElementById("sync-status-button").image =
        "chrome://browser/skin/sync-16-throbber.png";
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
    this._updateLastSyncItem();
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

  onNotificationAdded: function SUI_onNotificationAdded() {
    if (!gBrowser)
      return;

    let notificationsButton = document.getElementById("sync-notifications-button");
    notificationsButton.hidden = false;
    let notification = Weave.Notifications.notifications.reduce(function(prev, cur) {
      return prev.priority > cur.priority ? prev : cur;
    });

    let image = notification.priority >= Weave.Notifications.PRIORITY_WARNING ?
                "chrome://global/skin/icons/warning-16.png" :
                "chrome://global/skin/icons/information-16.png";
    notificationsButton.image = image;
    notificationsButton.label = notification.title;
  },

  onNotificationRemoved: function SUI_onNotificationRemoved() {
    if (!gBrowser)
      return;

    if (Weave.Notifications.notifications.length == 0) {
      document.getElementById("sync-notifications-button").hidden = true;
    }
    else {
      // Display remaining notifications
      this.onNotificationAdded();
    }
  },

  // Commands
  doUpdateMenu: function SUI_doUpdateMenu(event) {
    this._updateLastSyncItem();

    let loginItem = document.getElementById("sync-loginitem");
    let logoutItem = document.getElementById("sync-logoutitem");
    let syncItem = document.getElementById("sync-syncnowitem");

    // Don't allow "login" to be selected in some cases
    let offline = Services.io.offline;
    let locked = Weave.Service.locked;
    let noUser = Weave.Service.username == "";
    let notReady = offline || locked || noUser;
    loginItem.setAttribute("disabled", notReady);
    logoutItem.setAttribute("disabled", notReady);

    // Don't allow "sync now" to be selected in some cases
    let loggedIn = Weave.Service.isLoggedIn;
    let noNode = Weave.Status.sync == Weave.NO_SYNC_NODE_FOUND;
    let disableSync = notReady || !loggedIn || noNode;
    syncItem.setAttribute("disabled", disableSync);

    // Only show one of login/logout
    loginItem.setAttribute("hidden", loggedIn);
    logoutItem.setAttribute("hidden", !loggedIn);
  },

  doLogin: function SUI_doLogin() {
    Weave.Service.login();
  },

  doLogout: function SUI_doLogout() {
    Weave.Service.logout();
  },

  doSync: function SUI_doSync() {
    Weave.Service.sync();
  },

  handleStatusbarButton: function SUI_handleStatusbarButton() {
    if (Weave.Service.isLoggedIn)
      Weave.Service.sync();
    else if (this._needsSetup())
      this.openSetup();
    else
      Weave.Service.login();
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

  openPrefs: function SUI_openPrefs() {
    openPreferences("paneSync");
  },


  // Helpers
  _updateLastSyncItem: function SUI__updateLastSyncItem() {
    let lastSync;
    try {
      lastSync = Services.prefs.getCharPref("services.sync.lastSync");
    }
    catch (e) { };
    if (!lastSync)
      return;

    let lastSyncItem = document.getElementById("sync-lastsyncitem");

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSyncDate = new Date(lastSync).toLocaleFormat("%a %H:%M");
    let lastSyncLabel =
      this._stringBundle.formatStringFromName("lastSync.label", [lastSyncDate], 1);
    lastSyncItem.setAttribute("label", lastSyncLabel);
    lastSyncItem.setAttribute("hidden", "false");
    document.getElementById("sync-lastsyncsep").hidden = false;

    if (gBrowser)
      document.getElementById("sync-status-button").
               setAttribute("tooltiptext", lastSyncLabel);
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

      if (!Weave.Status.enforceBackoff) {
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
    this._updateLastSyncItem();
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
        this.onNotificationAdded();
        break;
      case "weave:notification:removed":
        this.onNotificationRemoved();
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

