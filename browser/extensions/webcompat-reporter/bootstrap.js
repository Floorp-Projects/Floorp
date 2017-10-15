/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global APP_SHUTDOWN:false */

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const WEBCOMPATREPORTER_JSM = "chrome://webcompat-reporter/content/WebCompatReporter.jsm";
const DELAYED_STARTUP_FINISHED = "browser-delayed-startup-finished";

XPCOMUtils.defineLazyModuleGetter(this, "WebCompatReporter",
  WEBCOMPATREPORTER_JSM);

const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";

function requestReporterInit() {
  Services.tm.idleDispatchToMainThread(function() {
    WebCompatReporter.init();
  });
}

function prefObserver(subject, topic, data) {
  let enabled = Services.prefs.getBoolPref(PREF_WC_REPORTER_ENABLED);
  if (enabled) {
    WebCompatReporter.init();
  } else {
    WebCompatReporter.uninit();
  }
}

function onDelayedStartupFinished(subject, topic, data) {
  requestReporterInit();
  Services.obs.removeObserver(onDelayedStartupFinished,
    DELAYED_STARTUP_FINISHED);
}

function startup(aData, aReason) {
  // Observe pref changes and enable/disable as necessary.
  Services.prefs.addObserver(PREF_WC_REPORTER_ENABLED, prefObserver);

  // Only initialize if pref is enabled, after the delayed startup notification.
  let enabled = Services.prefs.getBoolPref(PREF_WC_REPORTER_ENABLED);
  if (enabled) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win && win.gBrowserInit &&
        win.gBrowserInit.delayedStartupFinished) {
      requestReporterInit();
    } else {
      Services.obs.addObserver(onDelayedStartupFinished,
        DELAYED_STARTUP_FINISHED);
    }
  }
}

function shutdown(aData, aReason) {
  if (aReason === APP_SHUTDOWN) {
    return;
  }

  Cu.unload(WEBCOMPATREPORTER_JSM);
  Services.prefs.removeObserver(PREF_WC_REPORTER_ENABLED, prefObserver);
}

function install(aData, aReason) {}
function uninstall(aData, aReason) {}
