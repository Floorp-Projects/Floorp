/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SavantShieldStudy"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// See LOG_LEVELS in Console.jsm. Examples: "all", "info", "warn", & "error".
const PREF_LOG_LEVEL = "shield.savant.loglevel";

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "SavantShieldStudy",
  };
  return new ConsoleAPI(consoleOptions);
});

class SavantShieldStudyClass {
  constructor() {
    this.STUDY_PREF = "shield.savant.enabled";
    this.STUDY_TELEMETRY_CATEGORY = "savant";
    this.ALWAYS_PRIVATE_BROWSING_PREF = "browser.privatebrowsing.autostart";
  }

  init() {
    this.TelemetryEvents = new TelemetryEvents(this.STUDY_TELEMETRY_CATEGORY);

    // TODO check expiration, add study duration override pref

    // check the pref in case Normandy flipped it on before we could add the pref listener
    this.shouldCollect = Services.prefs.getBoolPref(this.STUDY_PREF);
    if (this.shouldCollect) {
      this.startupStudy();
    }
    Services.prefs.addObserver(this.STUDY_PREF, this);
  }

  observe(subject, topic, data) {
    if (topic === "nsPref:changed" && data === this.STUDY_PREF) {
      // toggle state of the pref
      this.shouldCollect = !this.shouldCollect;
      if (this.shouldCollect) {
        this.startupStudy();
      } else {
        // The pref has been turned off
        this.endStudy("study-disabled");
      }
    }
  }

  startupStudy() {
    if (!this.isEligible()) {
      this.endStudy("ineligible");
      return;
    }

    this.TelemetryEvents.enableCollection();
  }

  isEligible() {
    const isAlwaysPrivateBrowsing = Services.prefs.getBoolPref(this.ALWAYS_PRIVATE_BROWSING_PREF);
    if (isAlwaysPrivateBrowsing) {
      return false;
    }

    return true;
  }


  sendEvent(method, object, value, extra) {
    this.TelemetryEvents.sendEvent(method, object, value, extra);
  }

  endStudy(reason) {
    log.debug(`Ending the study due to reason: ${ reason }`);
    this.TelemetryEvents.disableCollection();
    // Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, "end_study", reason);
    this.uninit();
    // Study pref needs to persist between restarts, so only reset on endStudy
    Services.prefs.clearUserPref(this.STUDY_PREF);
  }

  // Called on every Firefox shutdown and endStudy
  uninit() {
    // TODO: clear study expiration override pref and remove its listener
    Services.prefs.removeObserver(this.ALWAYS_PRIVATE_BROWSING_PREF, this);
    Services.prefs.removeObserver(this.STUDY_PREF, this);
    Services.prefs.clearUserPref(PREF_LOG_LEVEL);
  }
}

const SavantShieldStudy = new SavantShieldStudyClass();

// references:
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/TelemetryEvents.jsm
// - https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/normandy/lib/PreferenceExperiments.jsm#l357
class TelemetryEvents {
  constructor(studyCategory) {
    this.STUDY_TELEMETRY_CATEGORY = studyCategory;
  }

  enableCollection() {
    log.debug("Study has been enabled; turning ON data collection.");
    Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, true);
  }

  disableCollection() {
    log.debug("Study has been disabled; turning OFF data collection.");
    Services.telemetry.setEventRecordingEnabled(this.STUDY_TELEMETRY_CATEGORY, false);
  }
}
