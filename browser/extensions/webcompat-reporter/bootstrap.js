/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const WEBCOMPATREPORTER_JSM = "chrome://webcompat-reporter/content/WebCompatReporter.jsm";

XPCOMUtils.defineLazyModuleGetter(this, "WebCompatReporter",
  WEBCOMPATREPORTER_JSM);

const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";

let prefObserver = function(aSubject, aTopic, aData) {
  let enabled = Services.prefs.getBoolPref(PREF_WC_REPORTER_ENABLED);
  if (enabled) {
    WebCompatReporter.init();
  } else {
    WebCompatReporter.uninit();
  }
};

function startup(aData, aReason) {
  // Observe pref changes and enable/disable as necessary.
  Services.prefs.addObserver(PREF_WC_REPORTER_ENABLED, prefObserver, false);

  // Only initialize if pref is enabled.
  let enabled = Services.prefs.getBoolPref(PREF_WC_REPORTER_ENABLED);
  if (enabled) {
    WebCompatReporter.init();
  }
}

function shutdown(aData, aReason) {
  if (aReason === APP_SHUTDOWN) {
    return;
  }

  Cu.unload(WEBCOMPATREPORTER_JSM);
}

function install(aData, aReason) {}
function uninstall(aData, aReason) {}
