/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ShieldStudySavant"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// See LOG_LEVELS in Console.jsm. Examples: "all", "info", "warn", & "error".
const PREF_LOG_LEVEL = "shield.savant.loglevel";

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "ShieldStudySavant",
  };
  return new ConsoleAPI(consoleOptions);
});

class ShieldStudySavantClass {
  constructor() {
    this.SHIELD_STUDY_SAVANT_PREF = "shield.savant.enabled";
    this.STUDY_TELEMETRY_CATEGORY = "savant";
  }

  init() {
    this.TelemetryEvents = new TelemetryEvents(this.STUDY_TELEMETRY_CATEGORY);

    // TODO: implement eligibility (#13)
    const isEligible = true;
    if (!isEligible) {
      this.endStudy("ineligible");
      return;
    }
    // check the pref in case Normandy flipped it on before we could add the pref listener
    this.shouldCollect = Services.prefs.getBoolPref(this.SHIELD_STUDY_SAVANT_PREF);
    if (this.shouldCollect) {
      this.TelemetryEvents.enableCollection();
    }
    Services.prefs.addObserver(this.SHIELD_STUDY_SAVANT_PREF, this);
  }

  observe(subject, topic, data) {
    if (topic === "nsPref:changed" && data === this.SHIELD_STUDY_SAVANT_PREF) {
      // toggle state of the pref
      this.shouldCollect = !this.shouldCollect;
      if (this.shouldCollect) {
        this.TelemetryEvents.enableCollection();
      } else {
        // The pref has been turned off
        this.endStudy("expired");
      }
    }
  }

  sendEvent(method, object, value, extra) {
    this.TelemetryEvents.sendEvent(method, object, value, extra);
  }

  endStudy(reason) {
    this.TelemetryEvents.disableCollection();
    // TODO: send endStudy ping with reason code
    this.uninit();
  }

  // Called on every Firefox shutdown and endStudy
  uninit() {
    // TODO: Make sure uninit() is called on every Firefox shutdown (look inside
    // nsBrowserGlue.js to see where Normandy uninits)
    // TODO: See what happens during Normandy's uninit method to ensure nothing
    // is forgotten.
    Services.prefs.removeObserver(this.SHIELD_STUDY_SAVANT_PREF, this);
    Services.prefs.clearUserPref(this.SHIELD_STUDY_SAVANT_PREF);
    Services.prefs.clearUserPref(PREF_LOG_LEVEL);
  }
}

const ShieldStudySavant = new ShieldStudySavantClass();

// references:
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/TelemetryEvents.jsm
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/PreferenceExperiments.jsm#l357
class TelemetryEvents {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
  }

  sendEvent(method, object, value, extra) {
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, method, object, value, extra);
  }

  enableCollection() {
    log.debug("Study has been enabled; turning ON data collection.");
    // Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, true);
  }

  disableCollection() {
    log.debug("Study has been disabled; turning OFF data collection.");
    // Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, false);
  }
}
