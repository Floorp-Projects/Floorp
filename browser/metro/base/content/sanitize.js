// -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "LoadContextInfo",
                                  "resource://gre/modules/LoadContextInfo.jsm");
function Sanitizer() {}

Sanitizer.prototype = {
  // warning to the caller: this one may raise an exception (e.g. bug #265028)
  clearItem: function (aItemName)
  {
    if (this.items[aItemName].canClear)
      this.items[aItemName].clear();
  },

  canClearItem: function (aItemName, aCallback, aArg)
  {
    let canClear = this.items[aItemName].canClear;
    if (typeof canClear == "function"){
      canClear(aCallback, aArg);
    } else {
      aCallback(aItemName, canClear, aArg);
    }
  },

  _prefDomain: "privacy.item.",
  getNameFromPreference: function (aPreferenceName)
  {
    return aPreferenceName.substr(this._prefDomain.length);
  },

  /**
   * Deletes privacy sensitive data in a batch, according to user preferences
   *
   * @returns  null if everything's fine;  an object in the form
   *           { itemName: error, ... } on (partial) failure
   */
  sanitize: function ()
  {
    var branch = Services.prefs.getBranch(this._prefDomain);
    var errors = null;
    for (var itemName in this.items) {
      if ("clear" in item && branch.getBoolPref(itemName)) {
        // Some of these clear() may raise exceptions (see bug #265028)
        // to sanitize as much as possible, we catch and store them, 
        // rather than fail fast.
        // Callers should check returned errors and give user feedback
        // about items that could not be sanitized
        let clearCallback = (itemName, aCanClear) => {
          let item = this.items[itemName];
          try{
            if (aCanClear){
              item.clear();
            }
          } catch(er){
            if (!errors){
              errors = {};
            }
            errors[itemName] = er;
            dump("Error sanitizing " + itemName + ":" + er + "\n");
          }
        }
        this.canClearItem(itemName, clearCallback);
      }
    }
    return errors;
  },

  items: {
    // Clear Sync account before passwords so that Sync still has access to the
    // credentials to clean up device-specific records on the server. Also
    // disable it before wiping history so we don't accidentally sync that.
    syncAccount: {
      clear: function ()
      {
        Sync.disconnect();
      },

      get canClear()
      {
        return (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED);
      }
    },

    cache: {
      clear: function ()
      {
        var cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
        try {
          cache.clear();
        } catch(er) {}

        let imageCache = Cc["@mozilla.org/image/cache;1"].getService(Ci.imgICache);
        try {
          imageCache.clearCache(false); // true=chrome, false=content
        } catch(er) {}
      },

      get canClear()
      {
        return true;
      }
    },

    cookies: {
      clear: function ()
      {
        var cookieMgr = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
        cookieMgr.removeAll();
      },

      get canClear()
      {
        return true;
      }
    },

    siteSettings: {
      clear: function ()
      {
        // Clear site-specific permissions like "Allow this site to open popups"
        Services.perms.removeAll();

        // Clear site-specific settings like page-zoom level
        var cps = Cc["@mozilla.org/content-pref/service;1"].getService(Ci.nsIContentPrefService2);
        cps.removeAllDomains(null);

        // Clear "Never remember passwords for this site", which is not handled by
        // the permission manager
        var pwmgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
        var hosts = pwmgr.getAllDisabledHosts({})
        for each (var host in hosts) {
          pwmgr.setLoginSavingEnabled(host, true);
        }
      },

      get canClear()
      {
        return true;
      }
    },

    offlineApps: {
      clear: function ()
      {
        var cacheService = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
        var appCacheStorage = cacheService.appCacheStorage(LoadContextInfo.default, null);
        try {
          appCacheStorage.asyncEvictStorage(null);
        } catch(er) {}
      },

      get canClear()
      {
          return true;
      }
    },

    history: {
      clear: function ()
      {
        try {
          Services.obs.notifyObservers(null, "browser:purge-session-history", "");
        }
        catch (e) {
          Components.utils.reportError("Failed to notify observers of "
                                     + "browser:purge-session-history: "
                                     + e);
        }
      },

      get canClear()
      {
        // bug 347231: Always allow clearing history due to dependencies on
        // the browser:purge-session-history notification. (like error console)
        return true;
      }
    },

    formdata: {
      clear: function ()
      {
        //Clear undo history of all searchBars
        var windows = Services.wm.getEnumerator("navigator:browser");
        while (windows.hasMoreElements()) {
          var searchBar = windows.getNext().document.getElementById("searchbar");
          if (searchBar) {
            searchBar.value = "";
            searchBar.textbox.editor.transactionManager.clear();
          }
        }
        FormHistory.update({op : "remove"});
      },

      canClear : function(aCallback, aArg)
      {
        let count = 0;
        let countDone = {
          handleResult : function(aResult) { count = aResult; },
          handleError : function(aError) { Components.utils.reportError(aError); },
          handleCompletion : function(aReason) { aCallback("formdata", aReason == 0 && count > 0, aArg); }
        };
        FormHistory.count({}, countDone);
      }
    },

    downloads: {
      clear: function ()
      {
        var dlMgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        dlMgr.cleanUp();
      },

      get canClear()
      {
        var dlMgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        return dlMgr.canCleanUp;
      }
    },

    passwords: {
      clear: function ()
      {
        var pwmgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
        pwmgr.removeAllLogins();
      },

      get canClear()
      {
        var pwmgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
        var count = pwmgr.countLogins("", "", ""); // count all logins
        return (count > 0);
      }
    },

    sessions: {
      clear: function ()
      {
        // clear all auth tokens
        var sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);
        sdr.logoutAndTeardown();

        // clear plain HTTP auth sessions
        var authMgr = Cc['@mozilla.org/network/http-auth-manager;1'].getService(Ci.nsIHttpAuthManager);
        authMgr.clearAll();
      },

      get canClear()
      {
        return true;
      }
    }
  }
};


// "Static" members
Sanitizer.prefDomain          = "privacy.sanitize.";
Sanitizer.prefShutdown        = "sanitizeOnShutdown";
Sanitizer.prefDidShutdown     = "didShutdownSanitize";

Sanitizer._prefs = null;
Sanitizer.__defineGetter__("prefs", function() 
{
  return Sanitizer._prefs ? Sanitizer._prefs
    : Sanitizer._prefs = Cc["@mozilla.org/preferences-service;1"]
                         .getService(Ci.nsIPrefService)
                         .getBranch(Sanitizer.prefDomain);
});

/** 
 * Deletes privacy sensitive data in a batch, optionally showing the 
 * sanitize UI, according to user preferences
 *
 * @returns  null if everything's fine
 *           an object in the form { itemName: error, ... } on (partial) failure
 */
Sanitizer.sanitize = function() 
{
  return new Sanitizer().sanitize();
};

Sanitizer.onStartup = function() 
{
  // we check for unclean exit with pending sanitization
  Sanitizer._checkAndSanitize();
};

Sanitizer.onShutdown = function() 
{
  // we check if sanitization is needed and perform it
  Sanitizer._checkAndSanitize();
};

// this is called on startup and shutdown, to perform pending sanitizations
Sanitizer._checkAndSanitize = function() 
{
  const prefs = Sanitizer.prefs;
  if (prefs.getBoolPref(Sanitizer.prefShutdown) && 
      !prefs.prefHasUserValue(Sanitizer.prefDidShutdown)) {
    // this is a shutdown or a startup after an unclean exit
    Sanitizer.sanitize() || // sanitize() returns null on full success
      prefs.setBoolPref(Sanitizer.prefDidShutdown, true);
  }
};


