/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component triggers an app update check even when system updates are
 * disabled to make sure we always check for app updates.
 */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/WebappsUpdater.jsm");

function debug(aStr) {
  //dump("--*-- WebappsUpdateTimer: " + aStr);
}

function WebappsUpdateTimer() {
}

WebappsUpdateTimer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback]),
  classID: Components.ID("{637b0f77-2429-49a0-915f-abf5d0db8b9a}"),

  notify: function(aTimer) {
    try {
      // We want to check app updates if system updates are disabled or
      // if they update frecency is not daily.
      if (Services.prefs.getBoolPref("app.update.enabled") === true &&
          Services.prefs.getIntPref("app.update.interval") === 86400) {
        return;
      }
    } catch(e) {
      // That should never happen..
    }

    // If we are offline, wait to be online to start the update check.
    if (Services.io.offline) {
      debug("Network is offline. Setting up an offline status observer.");
      Services.obs.addObserver(this, "network:offline-status-changed", false);
      return;
    }

    // This will trigger app updates in b2g/components/WebappsUpdater.jsm
    // that also takes care of notifying gaia.
    WebappsUpdater.updateApps();
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== "network:offline-status-changed" ||
        aData !== "online") {
      return;
    }

    debug("Network is online. Checking updates.");
    Services.obs.removeObserver(this, "network:offline-status-changed");
    WebappsUpdater.updateApps();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsUpdateTimer]);
