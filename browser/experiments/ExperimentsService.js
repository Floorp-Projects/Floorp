/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Experiments",
                                  "resource:///modules/experiments/Experiments.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

const PREF_EXPERIMENTS_ENABLED  = "experiments.enabled";
const PREF_HEALTHREPORT_ENABLED = "datareporting.healthreport.service.enabled";
const PREF_TELEMETRY_ENABLED    = "toolkit.telemetry.enabled";

XPCOMUtils.defineLazyGetter(
  this, "gExperimentsEnabled", () => {
    try {
      let prefs = Services.prefs;
      return prefs.getBoolPref(PREF_EXPERIMENTS_ENABLED) &&
             prefs.getBoolPref(PREF_TELEMETRY_ENABLED) &&
             prefs.getBoolPref(PREF_HEALTHREPORT_ENABLED);
    } catch (e) {
      return false;
    }
  });

function ExperimentsService() {
}

ExperimentsService.prototype = {
  classID: Components.ID("{f7800463-3b97-47f9-9341-b7617e6d8d49}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsIObserver]),

  notify: function (timer) {
    if (!gExperimentsEnabled) {
      return;
    }
    if (OS.Constants.Path.profileDir === undefined) {
      throw Error("Update timer fired before profile was initialized?");
    }
    Experiments.instance().updateManifest();
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        if (gExperimentsEnabled) {
          Experiments.instance(); // for side effects
        }
        break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExperimentsService]);
