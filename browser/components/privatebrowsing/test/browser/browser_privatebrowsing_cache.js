/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check about:cache after private browsing
// This test covers MozTrap test 6047
// bug 880621

let tmp = {};

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);

let Sanitizer = tmp.Sanitizer;

function test() {

  waitForExplicitFinish();

  sanitizeCache();

  let nrEntriesR1 = get_device_entry_count("disk");
  is (nrEntriesR1, 0, "Disk cache reports 0KB and has no entries");

  get_cache_for_private_window();
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
  return Components.classes["@mozilla.org/network/cache-service;1"]
                   .getService(Components.interfaces.nsICacheService);
}

function get_device_entry_count(device) {
  var cs = get_cache_service();
  var entry_count = -1;

  var visitor = {
    visitDevice: function (deviceID, deviceInfo) {
      if (device == deviceID) {
        entry_count = deviceInfo.entryCount;
      }
      return false;
    },
    visitEntry: function (deviceID, entryInfo) {
      do_throw("nsICacheVisitor.visitEntry should not be called " +
               "when checking the availability of devices");
    }
  };

  cs.visitEntries(visitor);
  return entry_count;
}

function get_cache_for_private_window () {
  let win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    executeSoon(function() {

      ok(true, "The private window got loaded");

      let tab = win.gBrowser.addTab("http://example.org");
      win.gBrowser.selectedTab = tab;
      let newTabBrowser = win.gBrowser.getBrowserForTab(tab);

      newTabBrowser.addEventListener("load", function eventHandler() {
        newTabBrowser.removeEventListener("load", eventHandler, true);

        executeSoon(function() {

          let nrEntriesP = get_device_entry_count("memory");
          is (nrEntriesP, 1, "Memory cache reports some entries from example.org domain");

          let nrEntriesR2 = get_device_entry_count("disk");
          is (nrEntriesR2, 0, "Disk cache reports 0KB and has no entries");

          cleanup();

          win.close();
          finish();
        });
      }, true);
    });
  }, false);
}
