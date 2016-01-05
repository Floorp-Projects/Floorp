/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Experiments",
                                  "resource:///modules/experiments/Experiments.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

const PREF_EXPERIMENTS_ENABLED  = "experiments.enabled";
const PREF_ACTIVE_EXPERIMENT    = "experiments.activeExperiment"; // whether we have an active experiment
const PREF_TELEMETRY_ENABLED    = "toolkit.telemetry.enabled";
const PREF_TELEMETRY_UNIFIED    = "toolkit.telemetry.unified";
const DELAY_INIT_MS             = 30 * 1000;

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Preferences.get(PREF_TELEMETRY_UNIFIED, false);

XPCOMUtils.defineLazyGetter(
  this, "gPrefs", () => {
    return new Preferences();
  });

XPCOMUtils.defineLazyGetter(
  this, "gExperimentsEnabled", () => {
    // We can enable experiments if either unified Telemetry or FHR is on, and the user
    // has opted into Telemetry.
    return gPrefs.get(PREF_EXPERIMENTS_ENABLED, false) &&
           IS_UNIFIED_TELEMETRY && gPrefs.get(PREF_TELEMETRY_ENABLED, false);
  });

XPCOMUtils.defineLazyGetter(
  this, "gActiveExperiment", () => {
    return gPrefs.get(PREF_ACTIVE_EXPERIMENT);
  });

function ExperimentsService() {
  this._initialized = false;
  this._delayedInitTimer = null;
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
    let instance = Experiments.instance();
    if (instance.isReady) {
      instance.updateManifest();
    }
  },

  _delayedInit: function () {
    if (!this._initialized) {
      this._initialized = true;
      Experiments.instance(); // for side effects
    }
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        if (gExperimentsEnabled) {
          Services.obs.addObserver(this, "quit-application", false);
          Services.obs.addObserver(this, "sessionstore-state-finalized", false);
          Services.obs.addObserver(this, "EM-loaded", false);

          if (gActiveExperiment) {
            this._initialized = true;
            Experiments.instance(); // for side effects
          }
        }
        break;
      case "sessionstore-state-finalized":
        if (!this._initialized) {
          CommonUtils.namedTimer(this._delayedInit, DELAY_INIT_MS, this, "_delayedInitTimer");
        }
        break;
      case "EM-loaded":
        if (!this._initialized) {
          Experiments.instance(); // for side effects
          this._initialized = true;

          if (this._delayedInitTimer) {
            this._delayedInitTimer.clear();
          }
        }
        break;
      case "quit-application":
        Services.obs.removeObserver(this, "quit-application");
        Services.obs.removeObserver(this, "sessionstore-state-finalized");
        Services.obs.removeObserver(this, "EM-loaded");
        if (this._delayedInitTimer) {
          this._delayedInitTimer.clear();
        }
        break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExperimentsService]);
