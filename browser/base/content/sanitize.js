# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Firefox Sanitizer.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Giorgio Maone <g.maone@informaction.com>
#   Johnathan Nightingale <johnath@mozilla.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

function Sanitizer() {}
Sanitizer.prototype = {
  // warning to the caller: this one may raise an exception (e.g. bug #265028)
  clearItem: function (aItemName)
  {
    if (this.items[aItemName].canClear)
      this.items[aItemName].clear();
  },

  canClearItem: function (aItemName)
  {
    return this.items[aItemName].canClear;
  },
  
  prefDomain: "",
  
  getNameFromPreference: function (aPreferenceName)
  {
    return aPreferenceName.substr(this.prefDomain.length);
  },
  
  /**
   * Deletes privacy sensitive data in a batch, according to user preferences
   *
   * @returns  null if everything's fine;  an object in the form
   *           { itemName: error, ... } on (partial) failure
   */
  sanitize: function ()
  {
    var psvc = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefService);
    var branch = psvc.getBranch(this.prefDomain);
    var errors = null;

    // Cache the range of times to clear
    if (this.ignoreTimespan)
      var range = null;  // If we ignore timespan, clear everything
    else
      range = this.range || Sanitizer.getClearRange();
      
    for (var itemName in this.items) {
      var item = this.items[itemName];
      item.range = range;
      if ("clear" in item && item.canClear && branch.getBoolPref(itemName)) {
        // Some of these clear() may raise exceptions (see bug #265028)
        // to sanitize as much as possible, we catch and store them, 
        // rather than fail fast.
        // Callers should check returned errors and give user feedback
        // about items that could not be sanitized
        try {
          item.clear();
        } catch(er) {
          if (!errors) 
            errors = {};
          errors[itemName] = er;
          dump("Error sanitizing " + itemName + ": " + er + "\n");
        }
      }
    }
    return errors;
  },
  
  // Time span only makes sense in certain cases.  Consumers who want
  // to only clear some private data can opt in by setting this to false,
  // and can optionally specify a specific range.  If timespan is not ignored,
  // and range is not set, sanitize() will use the value of the timespan
  // pref to determine a range
  ignoreTimespan : true,
  range : null,
  
  items: {
    cache: {
      clear: function ()
      {
        var cacheService = Cc["@mozilla.org/network/cache-service;1"].
                          getService(Ci.nsICacheService);
        try {
          // Cache doesn't consult timespan, nor does it have the
          // facility for timespan-based eviction.  Wipe it.
          cacheService.evictEntries(Ci.nsICache.STORE_ANYWHERE);
        } catch(er) {}

        var imageCache = Cc["@mozilla.org/image/cache;1"].
                         getService(Ci.imgICache);
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
        var cookieMgr = Components.classes["@mozilla.org/cookiemanager;1"]
                                  .getService(Ci.nsICookieManager);
        if (this.range) {
          // Iterate through the cookies and delete any created after our cutoff.
          var cookiesEnum = cookieMgr.enumerator;
          while (cookiesEnum.hasMoreElements()) {
            var cookie = cookiesEnum.getNext().QueryInterface(Ci.nsICookie2);
            
            if (cookie.creationTime > this.range[0])
              // This cookie was created after our cutoff, clear it
              cookieMgr.remove(cookie.host, cookie.name, cookie.path, false);
          }
        }
        else {
          // Remove everything
          cookieMgr.removeAll();
        }

        // Clear plugin data.
        const phInterface = Ci.nsIPluginHost;
        const FLAG_CLEAR_ALL = phInterface.FLAG_CLEAR_ALL;
        let ph = Cc["@mozilla.org/plugin/host;1"].getService(phInterface);

        // Determine age range in seconds. (-1 means clear all.) We don't know
        // that this.range[1] is actually now, so we compute age range based
        // on the lower bound. If this.range results in a negative age, do
        // nothing.
        let age = this.range ? (Date.now() / 1000 - this.range[0] / 1000000)
                             : -1;
        if (!this.range || age >= 0) {
          let tags = ph.getPluginTags();
          for (let i = 0; i < tags.length; i++) {
            try {
              ph.clearSiteData(tags[i], null, FLAG_CLEAR_ALL, age);
            } catch (e) {
              // If the plugin doesn't support clearing by age, clear everything.
              if (e.result == Components.results.
                    NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED) {
                try {
                  ph.clearSiteData(tags[i], null, FLAG_CLEAR_ALL, -1);
                } catch (e) {
                  // Ignore errors from the plugin
                }
              }
            }
          }
        }

        // clear any network geolocation provider sessions
        var psvc = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefService);
        try {
            var branch = psvc.getBranch("geo.wifi.access_token.");
            branch.deleteBranch("");
        } catch (e) {}

      },

      get canClear()
      {
        return true;
      }
    },

    offlineApps: {
      clear: function ()
      {
        const Cc = Components.classes;
        const Ci = Components.interfaces;
        var cacheService = Cc["@mozilla.org/network/cache-service;1"].
                           getService(Ci.nsICacheService);
        try {
          // Offline app data is "timeless", and doesn't respect
          // the setting of timespan, it always clears everything
          cacheService.evictEntries(Ci.nsICache.STORE_OFFLINE);
        } catch(er) {}

        var storageManagerService = Cc["@mozilla.org/dom/storagemanager;1"].
                                    getService(Ci.nsIDOMStorageManager);
        storageManagerService.clearOfflineApps();
      },

      get canClear()
      {
          return true;
      }
    },

    history: {
      clear: function ()
      {
        var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                      .getService(Components.interfaces.nsIBrowserHistory);
        if (this.range)
          globalHistory.removeVisitsByTimeframe(this.range[0], this.range[1]);
        else
          globalHistory.removeAllPages();
        
        try {
          var os = Components.classes["@mozilla.org/observer-service;1"]
                             .getService(Components.interfaces.nsIObserverService);
          os.notifyObservers(null, "browser:purge-session-history", "");
        }
        catch (e) { }
        
        // Clear last URL of the Open Web Location dialog
        var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch2);
        try {
          prefs.clearUserPref("general.open_location.last_url");
        }
        catch (e) { }
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
        // Clear undo history of all searchBars
        var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                      .getService(Components.interfaces.nsIWindowMediator);
        var windows = windowManager.getEnumerator("navigator:browser");
        while (windows.hasMoreElements()) {
          var searchBar = windows.getNext().document.getElementById("searchbar");
          if (searchBar)
            searchBar.textbox.reset();
        }

        var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                                    .getService(Components.interfaces.nsIFormHistory2);
        if (this.range)
          formHistory.removeEntriesByTimeframe(this.range[0], this.range[1]);
        else
          formHistory.removeAllEntries();
      },

      get canClear()
      {
        var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                      .getService(Components.interfaces.nsIWindowMediator);
        var windows = windowManager.getEnumerator("navigator:browser");
        while (windows.hasMoreElements()) {
          var searchBar = windows.getNext().document.getElementById("searchbar");
          if (searchBar) {
            var transactionMgr = searchBar.textbox.editor.transactionManager;
            if (searchBar.value ||
                transactionMgr.numberOfUndoItems ||
                transactionMgr.numberOfRedoItems)
              return true;
          }
        }

        var formHistory = Components.classes["@mozilla.org/satchel/form-history;1"]
                                    .getService(Components.interfaces.nsIFormHistory2);
        return formHistory.hasEntries;
      }
    },
    
    downloads: {
      clear: function ()
      {
        var dlMgr = Components.classes["@mozilla.org/download-manager;1"]
                              .getService(Components.interfaces.nsIDownloadManager);

        var dlIDsToRemove = [];
        if (this.range) {
          // First, remove the completed/cancelled downloads
          dlMgr.removeDownloadsByTimeframe(this.range[0], this.range[1]);
          
          // Queue up any active downloads that started in the time span as well
          var dlsEnum = dlMgr.activeDownloads;
          while(dlsEnum.hasMoreElements()) {
            var dl = dlsEnum.next();
            if(dl.startTime >= this.range[0])
              dlIDsToRemove.push(dl.id);
          }
        }
        else {
          // Clear all completed/cancelled downloads
          dlMgr.cleanUp();
          
          // Queue up all active ones as well
          var dlsEnum = dlMgr.activeDownloads;
          while(dlsEnum.hasMoreElements()) {
            dlIDsToRemove.push(dlsEnum.next().id);
          }
        }
        
        // Remove any queued up active downloads
        dlIDsToRemove.forEach(function(id) {
          dlMgr.removeDownload(id);
        });
      },

      get canClear()
      {
        var dlMgr = Components.classes["@mozilla.org/download-manager;1"]
                              .getService(Components.interfaces.nsIDownloadManager);
        return dlMgr.canCleanUp;
      }
    },
    
    passwords: {
      clear: function ()
      {
        var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
                              .getService(Components.interfaces.nsILoginManager);
        // Passwords are timeless, and don't respect the timeSpan setting
        pwmgr.removeAllLogins();
      },
      
      get canClear()
      {
        var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
                              .getService(Components.interfaces.nsILoginManager);
        var count = pwmgr.countLogins("", "", ""); // count all logins
        return (count > 0);
      }
    },
    
    sessions: {
      clear: function ()
      {
        // clear all auth tokens
        var sdr = Components.classes["@mozilla.org/security/sdr;1"]
                            .getService(Components.interfaces.nsISecretDecoderRing);
        sdr.logoutAndTeardown();

        // clear FTP and plain HTTP auth sessions
        var os = Components.classes["@mozilla.org/observer-service;1"]
                           .getService(Components.interfaces.nsIObserverService);
        os.notifyObservers(null, "net:clear-active-logins", null);
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
        var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                           .getService(Components.interfaces.nsIPermissionManager);
        pm.removeAll();
        
        // Clear site-specific settings like page-zoom level
        var cps = Components.classes["@mozilla.org/content-pref/service;1"]
                            .getService(Components.interfaces.nsIContentPrefService);
        cps.removeGroupedPrefs();
        
        // Clear "Never remember passwords for this site", which is not handled by
        // the permission manager
        var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
                              .getService(Components.interfaces.nsILoginManager);
        var hosts = pwmgr.getAllDisabledHosts();
        for each (var host in hosts) {
          pwmgr.setLoginSavingEnabled(host, true);
        }
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

// Time span constants corresponding to values of the privacy.sanitize.timeSpan
// pref.  Used to determine how much history to clear, for various items
Sanitizer.TIMESPAN_EVERYTHING = 0;
Sanitizer.TIMESPAN_HOUR       = 1;
Sanitizer.TIMESPAN_2HOURS     = 2;
Sanitizer.TIMESPAN_4HOURS     = 3;
Sanitizer.TIMESPAN_TODAY      = 4;

// Return a 2 element array representing the start and end times,
// in the uSec-since-epoch format that PRTime likes.  If we should
// clear everything, return null.  Use ts if it is defined; otherwise
// use the timeSpan pref.
Sanitizer.getClearRange = function (ts) {
  if (ts === undefined)
    ts = Sanitizer.prefs.getIntPref("timeSpan");
  if (ts === Sanitizer.TIMESPAN_EVERYTHING)
    return null;
  
  // PRTime is microseconds while JS time is milliseconds
  var endDate = Date.now() * 1000;
  switch (ts) {
    case Sanitizer.TIMESPAN_HOUR :
      var startDate = endDate - 3600000000; // 1*60*60*1000000
      break;
    case Sanitizer.TIMESPAN_2HOURS :
      startDate = endDate - 7200000000; // 2*60*60*1000000
      break;
    case Sanitizer.TIMESPAN_4HOURS :
      startDate = endDate - 14400000000; // 4*60*60*1000000
      break;
    case Sanitizer.TIMESPAN_TODAY :
      var d = new Date();  // Start with today
      d.setHours(0);      // zero us back to midnight...
      d.setMinutes(0);
      d.setSeconds(0);
      startDate = d.valueOf() * 1000; // convert to epoch usec
      break;
    default:
      throw "Invalid time span for clear private data: " + ts;
  }
  return [startDate, endDate];
};

Sanitizer._prefs = null;
Sanitizer.__defineGetter__("prefs", function() 
{
  return Sanitizer._prefs ? Sanitizer._prefs
    : Sanitizer._prefs = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefService)
                         .getBranch(Sanitizer.prefDomain);
});

// Shows sanitization UI
Sanitizer.showUI = function(aParentWindow) 
{
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);
#ifdef XP_MACOSX
  ww.openWindow(null, // make this an app-modal window on Mac
#else
  ww.openWindow(aParentWindow,
#endif
                "chrome://browser/content/sanitize.xul",
                "Sanitize",
                "chrome,titlebar,dialog,centerscreen,modal",
                null);
};

/** 
 * Deletes privacy sensitive data in a batch, optionally showing the 
 * sanitize UI, according to user preferences
 */
Sanitizer.sanitize = function(aParentWindow) 
{
  Sanitizer.showUI(aParentWindow);
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
    var s = new Sanitizer();
    s.prefDomain = "privacy.clearOnShutdown.";
    s.sanitize() || // sanitize() returns null on full success
      prefs.setBoolPref(Sanitizer.prefDidShutdown, true);
  }
};


