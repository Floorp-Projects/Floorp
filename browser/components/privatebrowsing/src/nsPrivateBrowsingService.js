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
# The Original Code is Private Browsing.
#
# The Initial Developer of the Original Code is
# Ehsan Akhgari.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Utilities

String.prototype.endsWith = function endsWith(aString)
{
  let index = this.indexOf(aString);
  // If it is not found, we know it doesn't end with it.
  if (index == -1)
    return false;

  // Otherwise, we end with the given string iff the index is aString.length
  // subtracted from our length.
  return index == (this.length - aString.length);
}

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

////////////////////////////////////////////////////////////////////////////////
//// PrivateBrowsingService

function PrivateBrowsingService() {
  this._obs.addObserver(this, "profile-after-change", true);
  this._obs.addObserver(this, "quit-application-granted", true);
}

PrivateBrowsingService.prototype = {
  // Observer Service
  __obs: null,
  get _obs() {
    if (!this.__obs)
      this.__obs = Cc["@mozilla.org/observer-service;1"].
                   getService(Ci.nsIObserverService);
    return this.__obs;
  },

  // Whether the private browsing mode is currently active or not.
  _inPrivateBrowsing: false,

  // Saved browser state before entering the private mode.
  _savedBrowserState: null,

  // Whether we're in the process of shutting down
  _quitting: false,

  // How to treat the non-private session
  _saveSession: true,

  // Make sure we don't allow re-enterant changing of the private mode
  _alreadyChangingMode: false,

  // Whether we're entering the private browsing mode at application startup
  _autoStart: false,

  // Whether the private browsing mode has been started automatically
  _autoStarted: false,

  // XPCOM registration
  classDescription: "PrivateBrowsing Service",
  contractID: "@mozilla.org/privatebrowsing;1",
  classID: Components.ID("{c31f4883-839b-45f6-82ad-a6a9bc5ad599}"),
  _xpcom_categories: [
    { category: "app-startup", service: true }
  ],

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrivateBrowsingService, 
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  _unload: function PBS__destroy() {
    // Force an exit from the private browsing mode on shutdown
    this._quitting = true;
    if (this._inPrivateBrowsing)
      this.privateBrowsingEnabled = false;
  },

  _onPrivateBrowsingModeChanged: function PBS__onPrivateBrowsingModeChanged() {
    // nothing needs to be done here if we're auto-starting
    if (!this._autoStart) {
      // clear all auth tokens
      let sdr = Cc["@mozilla.org/security/sdr;1"].
                getService(Ci.nsISecretDecoderRing);
      sdr.logoutAndTeardown();

      // clear plain HTTP auth sessions
      let authMgr = Cc['@mozilla.org/network/http-auth-manager;1'].
                    getService(Ci.nsIHttpAuthManager);
      authMgr.clearAll();

      let ss = Cc["@mozilla.org/browser/sessionstore;1"].
               getService(Ci.nsISessionStore);
      if (this.privateBrowsingEnabled) {
        // whether we should save and close the current session
        this._saveSession = true;
        var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                         getService(Ci.nsIPrefBranch);
        try {
          if (prefBranch.getBoolPref("browser.privatebrowsing.keep_current_session"))
            this._saveSession = false;
        } catch (ex) {}

        // save the whole browser state in order to restore all windows/tabs later
        if (this._saveSession && !this._savedBrowserState) {
          this._savedBrowserState = ss.getBrowserState();

          // Close all windows
          this._closeAllWindows();

          // Open about:privatebrowsing
          this._openAboutPrivateBrowsing();
        }
      }
      else {
        // Clear the error console
        let consoleService = Cc["@mozilla.org/consoleservice;1"].
                             getService(Ci.nsIConsoleService);
        consoleService.logStringMessage(null); // trigger the listeners
        consoleService.reset();

        // restore the windows/tabs which were open before entering the private mode
        if (this._saveSession && this._savedBrowserState) {
          if (!this._quitting) { // don't restore when shutting down!
            ss.setBrowserState(this._savedBrowserState);
          }
          this._savedBrowserState = null;
        }
      }
    }
    else
      this._saveSession = false;
  },

#ifndef XP_WIN
#define BROKEN_WM_Z_ORDER
#endif

  _closeAllWindows: function PBS__closeAllWindows() {
    let windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                         getService(Ci.nsIWindowMediator);
#ifdef BROKEN_WM_Z_ORDER
    let windowsEnum = windowMediator.getEnumerator("navigator:browser");
#else
    let windowsEnum = windowMediator.getZOrderDOMWindowEnumerator("navigator:browser", false);
#endif

    while (windowsEnum.hasMoreElements()) {
      let win = windowsEnum.getNext();
      win.close();
    }
  },

  _openAboutPrivateBrowsing: function PBS__openAboutPrivateBrowsing() {
    let windowWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                        getService(Ci.nsIWindowWatcher);
    let url = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
    url.data = "about:privatebrowsing";
    windowWatcher.openWindow(null, "chrome://browser/content/browser.xul",
                             null, "chrome,all,resizable=yes,dialog=no", url);
  },

  _canEnterPrivateBrowsingMode: function PBS__canEnterPrivateBrowsingMode() {
    let cancelEnter = Cc["@mozilla.org/supports-PRBool;1"].
                      createInstance(Ci.nsISupportsPRBool);
    cancelEnter.data = false;
    this._obs.notifyObservers(cancelEnter, "private-browsing-cancel-vote", "enter");
    return !cancelEnter.data;
  },

  _canLeavePrivateBrowsingMode: function PBS__canLeavePrivateBrowsingMode() {
    let cancelLeave = Cc["@mozilla.org/supports-PRBool;1"].
                      createInstance(Ci.nsISupportsPRBool);
    cancelLeave.data = false;
    this._obs.notifyObservers(cancelLeave, "private-browsing-cancel-vote", "exit");
    return !cancelLeave.data;
  },

  // nsIObserver

  observe: function PBS_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "profile-after-change":
        // If the autostart prefs has been set, simulate entering the
        // private browsing mode upon startup.
        // This won't interfere with the session store component, because
        // that component will be initialized on final-ui-startup.
        let prefsService = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefBranch);
        this._autoStart = prefsService.getBoolPref("browser.privatebrowsing.autostart");
        if (this._autoStart) {
          this._autoStarted = true;
          this.privateBrowsingEnabled = true;
          this._autoStart = false;
        }
        this._obs.removeObserver(this, "profile-after-change");
        break;
      case "quit-application-granted":
        this._unload();
        break;
    }
  },

  // nsIPrivateBrowsingService

  /**
   * Return the current status of private browsing.
   */
  get privateBrowsingEnabled PBS_get_privateBrowsingEnabled() {
    return this._inPrivateBrowsing;
  },

  /**
   * Enter or leave private browsing mode.
   */
  set privateBrowsingEnabled PBS_set_privateBrowsingEnabled(val) {
    // Allowing observers to set the private browsing status from their
    // notification handlers is not desired, because it will change the
    // status of the service while it's in the process of another transition.
    // So, we detect a reentrant call here and throw an error.
    // This is documented in nsIPrivateBrowsingService.idl.
    if (this._alreadyChangingMode)
      throw Cr.NS_ERROR_FAILURE;

    try {
      this._alreadyChangingMode = true;

      if (val != this._inPrivateBrowsing) {
        if (val) {
          if (!this._canEnterPrivateBrowsingMode())
            return;
        }
        else {
          if (!this._canLeavePrivateBrowsingMode())
            return;
        }

        if (!val)
          this._autoStarted = false;
        this._inPrivateBrowsing = val != false;

        let data = val ? "enter" : "exit";

        let quitting = Cc["@mozilla.org/supports-PRBool;1"].
                       createInstance(Ci.nsISupportsPRBool);
        quitting.data = this._quitting;
        this._obs.notifyObservers(quitting, "private-browsing", data);

        this._onPrivateBrowsingModeChanged();
      }
    } catch (ex) {
      Cu.reportError("Exception thrown while processing the " +
        "private browsing mode change request: " + ex.toString());
    } finally {
      this._alreadyChangingMode = false;
    }
  },

  /**
   * Whether private browsing has been started automatically.
   */
  get autoStarted PBS_get_autoStarted() {
    return this._autoStarted;
  },

  removeDataFromDomain: function PBS_removeDataFromDomain(aDomain)
  {
    // History
    let (bh = Cc["@mozilla.org/browser/global-history;2"].
              getService(Ci.nsIBrowserHistory)) {
      bh.removePagesFromHost(aDomain, true);
    }

    // Cache
    let (cs = Cc["@mozilla.org/network/cache-service;1"].
              getService(Ci.nsICacheService)) {
      // NOTE: there is no way to clear just that domain, so we clear out
      //       everything)
      cs.evictEntries(Ci.nsICache.STORE_ANYWHERE);
    }

    // Cookies
    let (cm = Cc["@mozilla.org/cookiemanager;1"].
              getService(Ci.nsICookieManager)) {
      let enumerator = cm.enumerator;
      while (enumerator.hasMoreElements()) {
        let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie);
        if (cookie.host.endsWith(aDomain))
          cm.remove(cookie.host, cookie.name, cookie.path, false);
      }
    }

    // Downloads
    let (dm = Cc["@mozilla.org/download-manager;1"].
              getService(Ci.nsIDownloadManager)) {
      // Active downloads
      let enumerator = dm.activeDownloads;
      while (enumerator.hasMoreElements()) {
        let dl = enumerator.getNext().QueryInterface(Ci.nsIDownload);
        if (dl.source.host.endsWith(aDomain)) {
          dm.cancelDownload(dl.id);
          dm.removeDownload(dl.id);
        }
      }

      // Completed downloads
      let db = dm.DBConnection;
      // NOTE: This is lossy, but we feel that it is OK to be lossy here and not
      //       invoke the cost of creating a URI for each download entry and
      //       ensure that the hostname matches.
      let stmt = db.createStatement(
        "DELETE FROM moz_downloads " +
        "WHERE source LIKE ?1 ESCAPE '/' " +
        "AND state NOT IN (?2, ?3, ?4)"
      );
      let pattern = stmt.escapeStringForLIKE(aDomain, "/");
      stmt.bindStringParameter(0, "%" + pattern + "%");
      stmt.bindInt32Parameter(1, Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING);
      stmt.bindInt32Parameter(2, Ci.nsIDownloadManager.DOWNLOAD_PAUSED);
      stmt.bindInt32Parameter(3, Ci.nsIDownloadManager.DOWNLOAD_QUEUED);
      try {
        stmt.execute();
      }
      finally {
        stmt.finalize();
      }

      // We want to rebuild the list if the UI is showing, so dispatch the
      // observer topic
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.notifyObservers(null, "download-manager-remove-download", null);
    }

    // Passwords
    let (lm = Cc["@mozilla.org/login-manager;1"].
              getService(Ci.nsILoginManager)) {
      // Clear all passwords for domain
      try {
        let logins = lm.getAllLogins({});
        for (let i = 0; i < logins.length; i++)
          if (logins[i].hostname.endsWith(aDomain))
            lm.removeLogin(logins[i]);
      }
      // XXXehsan: is there a better way to do this rather than this
      // hacky comparison?
      catch (ex if ex == "User canceled Master Password entry") {}

      // Clear any "do not save for this site" for this domain
      let disabledHosts = lm.getAllDisabledHosts({});
      for (let i = 0; i < disabledHosts.length; i++)
        if (disabledHosts[i].endsWith(aDomain))
          lm.setLoginSavingEnabled(disabledHosts, true);
    }

    // Permissions
    let (pm = Cc["@mozilla.org/permissionmanager;1"].
              getService(Ci.nsIPermissionManager)) {
      // Enumerate all of the permissions, and if one matches, remove it
      let enumerator = pm.enumerator;
      while (enumerator.hasMoreElements()) {
        let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);
        if (perm.host.endsWith(aDomain))
          pm.remove(perm.host, perm.type);
      }
    }

    // Content Preferences
    let (cp = Cc["@mozilla.org/content-pref/service;1"].
              getService(Ci.nsIContentPrefService)) {
      let db = cp.DBConnection;
      // First we need to get the list of "groups" which are really just domains
      let names = [];
      let stmt = db.createStatement(
        "SELECT name " +
        "FROM groups " +
        "WHERE name LIKE ?1 ESCAPE '/'"
      );
      let pattern = stmt.escapeStringForLIKE(aDomain, "/");
      stmt.bindStringParameter(0, "%" + pattern);
      try {
        while (stmt.executeStep())
          names.push(stmt.getString(0));
      }
      finally {
        stmt.finalize();
      }

      // Now, for each name we got back, remove all of its prefs.
      for (let i = 0; i < names.length; i++) {
        // The service only cares about the host of the URI, so we don't need a
        // full nsIURI object here.
        let uri = { host: names[i]};
        let enumerator = cp.getPrefs(uri).enumerator;
        while (enumerator.hasMoreElements()) {
          let pref = enumerator.getNext().QueryInterface(Ci.nsIProperty);
          cp.removePref(uri, pref.name);
        }
      }
    }
  }
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([PrivateBrowsingService]);
