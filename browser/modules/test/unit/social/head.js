/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var Social, SocialService;

var manifests = [
  {
    name: "provider 1",
    origin: "https://example1.com",
    sidebarURL: "https://example1.com/sidebar/",
  },
  {
    name: "provider 2",
    origin: "https://example2.com",
    sidebarURL: "https://example1.com/sidebar/",
  }
];

const MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");

// SocialProvider class relies on blocklisting being enabled.  To enable
// blocklisting, we have to setup an app and initialize the blocklist (see
// initApp below).
const gProfD = do_get_profile();

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

function createAppInfo(id, name, version, platformVersion) {
  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion ? platformVersion : "1.0",
    platformBuildID: "2007010101",

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                           Ci.nsIXULRuntime,
                                           Ci.nsICrashReporter,
                                           Ci.nsISupports])
  };

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

function initApp() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  // prepare a blocklist file for the blocklist service
  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("blocklist.xml");
  source.copyTo(gProfD, "blocklist.xml");
  blocklistFile.lastModifiedTime = Date.now();


  let internalManager = Cc["@mozilla.org/addons/integration;1"].
                     getService(Ci.nsIObserver).
                     QueryInterface(Ci.nsITimerCallback);

  internalManager.observe(null, "addons-startup", null);
}

function setManifestPref(manifest) {
  let string = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(manifest);
  Services.prefs.setComplexValue("social.manifest." + manifest.origin, Ci.nsISupportsString, string);
}

function do_wait_observer(topic, cb) {
  function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    cb();
  }
  Services.obs.addObserver(observer, topic, false);
}

function do_add_providers(cb) {
  // run only after social is already initialized
  SocialService.addProvider(manifests[0], function() {
    do_wait_observer("social:providers-changed", function() {
      do_check_eq(Social.providers.length, 2, "2 providers installed");
      do_execute_soon(cb);
    });
    SocialService.addProvider(manifests[1]);
  });
}

function do_initialize_social(enabledOnStartup, cb) {
  initApp();

  if (enabledOnStartup) {
    // set prefs before initializing social
    manifests.forEach(function (manifest) {
      setManifestPref(manifest);
    });
    // Set both providers active and flag the first one as "current"
    let activeVal = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
    let active = {};
    for (let m of manifests)
      active[m.origin] = 1;
    activeVal.data = JSON.stringify(active);
    Services.prefs.setComplexValue("social.activeProviders",
                                   Ci.nsISupportsString, activeVal);

    do_register_cleanup(function() {
      manifests.forEach(function (manifest) {
        Services.prefs.clearUserPref("social.manifest." + manifest.origin);
      });
      Services.prefs.clearUserPref("social.activeProviders");
    });

    // expecting 2 providers installed
    do_wait_observer("social:providers-changed", function() {
      do_check_eq(Social.providers.length, 2, "2 providers installed");
      do_execute_soon(cb);
    });
  }

  // import and initialize everything
  SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;
  do_check_eq(enabledOnStartup, SocialService.hasEnabledProviders, "Service has enabled providers");
  Social = Cu.import("resource:///modules/Social.jsm", {}).Social;
  do_check_false(Social.initialized, "Social is not initialized");
  Social.init();
  do_check_true(Social.initialized, "Social is initialized");
  if (!enabledOnStartup)
    do_execute_soon(cb);
}
