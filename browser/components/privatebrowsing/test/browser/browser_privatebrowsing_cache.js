/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check about:cache after private browsing
// This test covers MozTrap test 6047
// bug 880621

var {LoadContextInfo} = Cu.import("resource://gre/modules/LoadContextInfo.jsm", null);

var tmp = {};

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);

var Sanitizer = tmp.Sanitizer;

function test() {

  waitForExplicitFinish();

  sanitizeCache();

  let nrEntriesR1 = getStorageEntryCount("regular", function(nrEntriesR1) {
    is(nrEntriesR1, 0, "Disk cache reports 0KB and has no entries");

    get_cache_for_private_window();
  });
}

function cleanup() {
  let prefs = Services.prefs.getBranch("privacy.cpd.");

  prefs.clearUserPref("history");
  prefs.clearUserPref("downloads");
  prefs.clearUserPref("cache");
  prefs.clearUserPref("cookies");
  prefs.clearUserPref("formdata");
  prefs.clearUserPref("offlineApps");
  prefs.clearUserPref("passwords");
  prefs.clearUserPref("sessions");
  prefs.clearUserPref("siteSettings");
}

function sanitizeCache() {

  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";

  let prefs = gPrefService.getBranch(s.prefDomain);
  prefs.setBoolPref("history", false);
  prefs.setBoolPref("downloads", false);
  prefs.setBoolPref("cache", true);
  prefs.setBoolPref("cookies", false);
  prefs.setBoolPref("formdata", false);
  prefs.setBoolPref("offlineApps", false);
  prefs.setBoolPref("passwords", false);
  prefs.setBoolPref("sessions", false);
  prefs.setBoolPref("siteSettings", false);

  s.sanitize();
}

function get_cache_service() {
  return Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                   .getService(Components.interfaces.nsICacheStorageService);
}

function getStorageEntryCount(device, goon) {
  var cs = get_cache_service();

  var storage;
  switch (device) {
  case "private":
    storage = cs.diskCacheStorage(LoadContextInfo.private, false);
    break;
  case "regular":
    storage = cs.diskCacheStorage(LoadContextInfo.default, false);
    break;
  default:
    throw "Unknown device " + device + " at getStorageEntryCount";
  }

  var visitor = {
    entryCount: 0,
    onCacheStorageInfo: function (aEntryCount, aConsumption) {
    },
    onCacheEntryInfo: function(uri)
    {
      var urispec = uri.asciiSpec;
      info(device + ":" + urispec + "\n");
      if (urispec.match(/^http:\/\/example.org\//))
        ++this.entryCount;
    },
    onCacheEntryVisitCompleted: function()
    {
      goon(this.entryCount);
    }
  };

  storage.asyncVisitStorage(visitor, true);
}

function get_cache_for_private_window () {
  let win = whenNewWindowLoaded({private: true}, function() {

    executeSoon(function() {

      ok(true, "The private window got loaded");

      let tab = win.gBrowser.addTab("http://example.org");
      win.gBrowser.selectedTab = tab;
      let newTabBrowser = win.gBrowser.getBrowserForTab(tab);

      newTabBrowser.addEventListener("load", function eventHandler() {
        newTabBrowser.removeEventListener("load", eventHandler, true);

        executeSoon(function() {

          getStorageEntryCount("private", function(nrEntriesP) {
            ok(nrEntriesP >= 1, "Memory cache reports some entries from example.org domain");

            getStorageEntryCount("regular", function(nrEntriesR2) {
              is(nrEntriesR2, 0, "Disk cache reports 0KB and has no entries");

              cleanup();

              win.close();
              finish();
            });
          });
        });
      }, true);
    });
  });
}
