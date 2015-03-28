# -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");

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
    if (typeof canClear == "function") {
      canClear(aCallback, aArg);
      return false;
    }

    aCallback(aItemName, canClear, aArg);
    return canClear;
  },

  prefDomain: "",

  getNameFromPreference: function (aPreferenceName)
  {
    return aPreferenceName.substr(this.prefDomain.length);
  },

  /**
   * Deletes privacy sensitive data in a batch, according to user preferences.
   * Returns a promise which is resolved if no errors occurred.  If an error
   * occurs, a message is reported to the console and all other items are still
   * cleared before the promise is finally rejected.
   *
   * If the consumer specifies the (optional) array parameter, only those
   * items get cleared (irrespective of the preference settings)
   */
  sanitize: function (aItemsToClear)
  {
    var deferred = Promise.defer();
    var seenError = false;
    if (Array.isArray(aItemsToClear)) {
      var itemsToClear = [...aItemsToClear];
    } else {
      let branch = Services.prefs.getBranch(this.prefDomain);
      itemsToClear = Object.keys(this.items).filter(itemName => branch.getBoolPref(itemName));
    }

    // Ensure open windows get cleared first, if they're in our list, so that they don't stick
    // around in the recently closed windows list, and so we can cancel the whole thing
    // if the user selects to keep a window open from a beforeunload prompt.
    let openWindowsIndex = itemsToClear.indexOf("openWindows");
    if (openWindowsIndex != -1) {
      itemsToClear.splice(openWindowsIndex, 1);
      let item = this.items.openWindows;

      let ok = item.clear(() => {
        try {
          let clearedPromise = this.sanitize(itemsToClear);
          clearedPromise.then(deferred.resolve, deferred.reject);
        } catch(e) {
          let error = "Sanitizer threw after closing windows: " + e;
          Cu.reportError(error);
          deferred.reject(error);
        }
      });
      // When cancelled, reject immediately
      if (!ok) {
        deferred.reject("Sanitizer canceled closing windows");
      }

      return deferred.promise;
    }

    TelemetryStopwatch.start("FX_SANITIZE_TOTAL");

    // Cache the range of times to clear
    if (this.ignoreTimespan)
      var range = null;  // If we ignore timespan, clear everything
    else
      range = this.range || Sanitizer.getClearRange();

    let itemCount = Object.keys(itemsToClear).length;
    let onItemComplete = function() {
      if (!--itemCount) {
        TelemetryStopwatch.finish("FX_SANITIZE_TOTAL");
        seenError ? deferred.reject() : deferred.resolve();
      }
    };
    for (let itemName of itemsToClear) {
      let item = this.items[itemName];
      item.range = range;
      if ("clear" in item) {
        let clearCallback = (itemName, aCanClear) => {
          // Some of these clear() may raise exceptions (see bug #265028)
          // to sanitize as much as possible, we catch and store them,
          // rather than fail fast.
          // Callers should check returned errors and give user feedback
          // about items that could not be sanitized
          let item = this.items[itemName];
          try {
            if (aCanClear)
              item.clear();
          } catch(er) {
            seenError = true;
            Components.utils.reportError("Error sanitizing " + itemName +
                                         ": " + er + "\n");
          }
          onItemComplete();
        };
        this.canClearItem(itemName, clearCallback);
      } else {
        onItemComplete();
      }
    }

    return deferred.promise;
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
        TelemetryStopwatch.start("FX_SANITIZE_CACHE");

        var cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"].
                    getService(Ci.nsICacheStorageService);
        try {
          // Cache doesn't consult timespan, nor does it have the
          // facility for timespan-based eviction.  Wipe it.
          cache.clear();
        } catch(er) {}

        var imageCache = Cc["@mozilla.org/image/tools;1"].
                         getService(Ci.imgITools).getImgCacheForDocument(null);
        try {
          imageCache.clearCache(false); // true=chrome, false=content
        } catch(er) {}

        TelemetryStopwatch.finish("FX_SANITIZE_CACHE");
      },

      get canClear()
      {
        return true;
      }
    },

    cookies: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_COOKIES");

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

        TelemetryStopwatch.finish("FX_SANITIZE_COOKIES");
      },

      get canClear()
      {
        return true;
      }
    },

    offlineApps: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_OFFLINEAPPS");
        Components.utils.import("resource:///modules/offlineAppCache.jsm");
        OfflineAppCacheHelper.clear();
        TelemetryStopwatch.finish("FX_SANITIZE_OFFLINEAPPS");
      },

      get canClear()
      {
        return true;
      }
    },

    history: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_HISTORY");

        if (this.range)
          PlacesUtils.history.removeVisitsByTimeframe(this.range[0], this.range[1]);
        else
          PlacesUtils.history.removeAllPages();

        try {
          var os = Components.classes["@mozilla.org/observer-service;1"]
                             .getService(Components.interfaces.nsIObserverService);
          let clearStartingTime = this.range ? String(this.range[0]) : "";
          os.notifyObservers(null, "browser:purge-session-history", clearStartingTime);
        }
        catch (e) { }

        try {
          var predictor = Components.classes["@mozilla.org/network/predictor;1"]
                                    .getService(Components.interfaces.nsINetworkPredictor);
          predictor.reset();
        } catch (e) { }

        TelemetryStopwatch.finish("FX_SANITIZE_HISTORY");
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
        TelemetryStopwatch.start("FX_SANITIZE_FORMDATA");

        // Clear undo history of all searchBars
        var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                      .getService(Components.interfaces.nsIWindowMediator);
        var windows = windowManager.getEnumerator("navigator:browser");
        while (windows.hasMoreElements()) {
          let currentWindow = windows.getNext();
          let currentDocument = currentWindow.document;
          let searchBar = currentDocument.getElementById("searchbar");
          if (searchBar)
            searchBar.textbox.reset();
          let tabBrowser = currentWindow.gBrowser;
          for (let tab of tabBrowser.tabs) {
            if (tabBrowser.isFindBarInitialized(tab))
              tabBrowser.getFindBar(tab).clear();
          }
          // Clear any saved find value
          tabBrowser._lastFindValue = "";
        }

        let change = { op: "remove" };
        if (this.range) {
          [ change.firstUsedStart, change.firstUsedEnd ] = this.range;
        }
        FormHistory.update(change);

        TelemetryStopwatch.finish("FX_SANITIZE_FORMDATA");
      },

      canClear : function(aCallback, aArg)
      {
        var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1']
                                      .getService(Components.interfaces.nsIWindowMediator);
        var windows = windowManager.getEnumerator("navigator:browser");
        while (windows.hasMoreElements()) {
          let currentWindow = windows.getNext();
          let currentDocument = currentWindow.document;
          let searchBar = currentDocument.getElementById("searchbar");
          if (searchBar) {
            let transactionMgr = searchBar.textbox.editor.transactionManager;
            if (searchBar.value ||
                transactionMgr.numberOfUndoItems ||
                transactionMgr.numberOfRedoItems) {
              aCallback("formdata", true, aArg);
              return false;
            }
          }
          let tabBrowser = currentWindow.gBrowser;
          let findBarCanClear = Array.some(tabBrowser.tabs, function (aTab) {
            return tabBrowser.isFindBarInitialized(aTab) &&
                   tabBrowser.getFindBar(aTab).canClear;
          });
          if (findBarCanClear) {
            aCallback("formdata", true, aArg);
            return false;
          }
        }

        let count = 0;
        let countDone = {
          handleResult : function(aResult) count = aResult,
          handleError : function(aError) Components.utils.reportError(aError),
          handleCompletion :
            function(aReason) { aCallback("formdata", aReason == 0 && count > 0, aArg); }
        };
        FormHistory.count({}, countDone);
        return false;
      }
    },

    downloads: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_DOWNLOADS");
        Task.spawn(function () {
          let filterByTime = null;
          if (this.range) {
            // Convert microseconds back to milliseconds for date comparisons.
            let rangeBeginMs = this.range[0] / 1000;
            let rangeEndMs = this.range[1] / 1000;
            filterByTime = download => download.startTime >= rangeBeginMs &&
                                       download.startTime <= rangeEndMs;
          }

          // Clear all completed/cancelled downloads
          let list = yield Downloads.getList(Downloads.ALL);
          list.removeFinished(filterByTime);
          TelemetryStopwatch.finish("FX_SANITIZE_DOWNLOADS");
        }.bind(this)).then(null, error => {
          TelemetryStopwatch.finish("FX_SANITIZE_DOWNLOADS");
          Components.utils.reportError(error);
        });
      },

      canClear : function(aCallback, aArg)
      {
        aCallback("downloads", true, aArg);
        return false;
      }
    },

    passwords: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_PASSWORDS");
        var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
                              .getService(Components.interfaces.nsILoginManager);
        // Passwords are timeless, and don't respect the timeSpan setting
        pwmgr.removeAllLogins();
        TelemetryStopwatch.finish("FX_SANITIZE_PASSWORDS");
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
        TelemetryStopwatch.start("FX_SANITIZE_SESSIONS");

        // clear all auth tokens
        var sdr = Components.classes["@mozilla.org/security/sdr;1"]
                            .getService(Components.interfaces.nsISecretDecoderRing);
        sdr.logoutAndTeardown();

        // clear FTP and plain HTTP auth sessions
        var os = Components.classes["@mozilla.org/observer-service;1"]
                           .getService(Components.interfaces.nsIObserverService);
        os.notifyObservers(null, "net:clear-active-logins", null);

        TelemetryStopwatch.finish("FX_SANITIZE_SESSIONS");
      },

      get canClear()
      {
        return true;
      }
    },

    siteSettings: {
      clear: function ()
      {
        TelemetryStopwatch.start("FX_SANITIZE_SITESETTINGS");

        // Clear site-specific permissions like "Allow this site to open popups"
        // we ignore the "end" range and hope it is now() - none of the
        // interfaces used here support a true range anyway.
        let startDateMS = this.range == null ? null : this.range[0] / 1000;
        var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                           .getService(Components.interfaces.nsIPermissionManager);
        if (startDateMS == null) {
          pm.removeAll();
        } else {
          pm.removeAllSince(startDateMS);
        }

        // Clear site-specific settings like page-zoom level
        var cps = Components.classes["@mozilla.org/content-pref/service;1"]
                            .getService(Components.interfaces.nsIContentPrefService2);
        if (startDateMS == null) {
          cps.removeAllDomains(null);
        } else {
          cps.removeAllDomainsSince(startDateMS, null);
        }

        // Clear "Never remember passwords for this site", which is not handled by
        // the permission manager
        // (Note the login manager doesn't support date ranges yet, and bug
        //  1058438 is calling for loginSaving stuff to end up in the
        // permission manager)
        var pwmgr = Components.classes["@mozilla.org/login-manager;1"]
                              .getService(Components.interfaces.nsILoginManager);
        var hosts = pwmgr.getAllDisabledHosts();
        for each (var host in hosts) {
          pwmgr.setLoginSavingEnabled(host, true);
        }

        // Clear site security settings - no support for ranges in this
        // interface either, so we clearAll().
        var sss = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
        sss.clearAll();

        TelemetryStopwatch.finish("FX_SANITIZE_SITESETTINGS");
      },

      get canClear()
      {
        return true;
      }
    },
    openWindows: {
      privateStateForNewWindow: "non-private",
      _canCloseWindow: function(aWindow) {
        // Bug 967873 - Proxy nsDocumentViewer::PermitUnload to the child process
        if (!aWindow.gMultiProcessBrowser) {
          // Cargo-culted out of browser.js' WindowIsClosing because we don't care
          // about TabView or the regular 'warn me before closing windows with N tabs'
          // stuff here, and more importantly, we want to set aCallerClosesWindow to true
          // when calling into permitUnload:
          for (let browser of aWindow.gBrowser.browsers) {
            let ds = browser.docShell;
            // 'true' here means we will be closing the window soon, so please don't dispatch
            // another onbeforeunload event when we do so. If unload is *not* permitted somewhere,
            // we will reset the flag that this triggers everywhere so that we don't interfere
            // with the browser after all:
            if (ds.contentViewer && !ds.contentViewer.permitUnload(true)) {
              return false;
            }
          }
        }
        return true;
      },
      _resetAllWindowClosures: function(aWindowList) {
        for (let win of aWindowList) {
          win.getInterface(Ci.nsIDocShell).contentViewer.resetCloseWindow();
        }
      },
      clear: function(aCallback)
      {
        // NB: this closes all *browser* windows, not other windows like the library, about window,
        // browser console, etc.

        if (!aCallback) {
          throw "Sanitizer's openWindows clear() requires a callback.";
        }

        // Keep track of the time in case we get stuck in la-la-land because of onbeforeunload
        // dialogs
        let existingWindow = Services.appShell.hiddenDOMWindow;
        let startDate = existingWindow.performance.now();

        // First check if all these windows are OK with being closed:
        let windowEnumerator = Services.wm.getEnumerator("navigator:browser");
        let windowList = [];
        while (windowEnumerator.hasMoreElements()) {
          let someWin = windowEnumerator.getNext();
          windowList.push(someWin);
          // If someone says "no" to a beforeunload prompt, we abort here:
          if (!this._canCloseWindow(someWin)) {
            this._resetAllWindowClosures(windowList);
            return false;
          }

          // ...however, beforeunload prompts spin the event loop, and so the code here won't get
          // hit until the prompt has been dismissed. If more than 1 minute has elapsed since we
          // started prompting, stop, because the user might not even remember initiating the
          // 'forget', and the timespans will be all wrong by now anyway:
          if (existingWindow.performance.now() > (startDate + 60 * 1000)) {
            this._resetAllWindowClosures(windowList);
            return false;
          }
        }

        // If/once we get here, we should actually be able to close all windows.

        TelemetryStopwatch.start("FX_SANITIZE_OPENWINDOWS");

        // First create a new window. We do this first so that on non-mac, we don't
        // accidentally close the app by closing all the windows.
        let handler = Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler);
        let defaultArgs = handler.defaultArgs;
        let features = "chrome,all,dialog=no," + this.privateStateForNewWindow;
        let newWindow = existingWindow.openDialog("chrome://browser/content/", "_blank",
                                                  features, defaultArgs);
#ifdef XP_MACOSX
        function onFullScreen(e) {
          newWindow.removeEventListener("fullscreen", onFullScreen);
          let docEl = newWindow.document.documentElement;
          let sizemode = docEl.getAttribute("sizemode");
          if (!newWindow.fullScreen && sizemode == "fullscreen") {
            docEl.setAttribute("sizemode", "normal");
            e.preventDefault();
            e.stopPropagation();
            return false;
          }
        }
        newWindow.addEventListener("fullscreen", onFullScreen);
#endif

        // Window creation and destruction is asynchronous. We need to wait
        // until all existing windows are fully closed, and the new window is
        // fully open, before continuing. Otherwise the rest of the sanitizer
        // could run too early (and miss new cookies being set when a page
        // closes) and/or run too late (and not have a fully-formed window yet
        // in existence). See bug 1088137.
        let newWindowOpened = false;
        function onWindowOpened(subject, topic, data) {
          if (subject != newWindow)
            return;

          Services.obs.removeObserver(onWindowOpened, "browser-delayed-startup-finished");
#ifdef XP_MACOSX
          newWindow.removeEventListener("fullscreen", onFullScreen);
#endif
          newWindowOpened = true;
          // If we're the last thing to happen, invoke callback.
          if (numWindowsClosing == 0) {
            TelemetryStopwatch.finish("FX_SANITIZE_OPENWINDOWS");
            aCallback();
          }
        }

        let numWindowsClosing = windowList.length;
        function onWindowClosed() {
          numWindowsClosing--;
          if (numWindowsClosing == 0) {
            Services.obs.removeObserver(onWindowClosed, "xul-window-destroyed");
            // If we're the last thing to happen, invoke callback.
            if (newWindowOpened) {
              TelemetryStopwatch.finish("FX_SANITIZE_OPENWINDOWS");
              aCallback();
            }
          }
        }

        Services.obs.addObserver(onWindowOpened, "browser-delayed-startup-finished", false);
        Services.obs.addObserver(onWindowClosed, "xul-window-destroyed", false);

        // Start the process of closing windows
        while (windowList.length) {
          windowList.pop().close();
        }
        newWindow.focus();
        return true;
      },

      get canClear()
      {
        return true;
      }
    },
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
Sanitizer.TIMESPAN_5MIN       = 5;
Sanitizer.TIMESPAN_24HOURS    = 6;

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
    case Sanitizer.TIMESPAN_5MIN :
      var startDate = endDate - 300000000; // 5*60*1000000
      break;
    case Sanitizer.TIMESPAN_HOUR :
      startDate = endDate - 3600000000; // 1*60*60*1000000
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
    case Sanitizer.TIMESPAN_24HOURS :
      startDate = endDate - 86400000000; // 24*60*60*1000000
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
    s.sanitize().then(function() {
      prefs.setBoolPref(Sanitizer.prefDidShutdown, true);
    });
  }
};
