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
    this.STUDY_DURATION_OVERRIDE_PREF = "shield.savant.duration_override";
    this.STUDY_EXPIRATION_DATE_PREF = "shield.savant.expiration_date";
    // ms = 'x' weeks * 7 days/week * 24 hours/day * 60 minutes/hour
    // * 60 seconds/minute * 1000 milliseconds/second
    this.DEFAULT_STUDY_DURATION_MS = 4 * 7 * 24 * 60 * 60 * 1000;
  }

  init() {
    this.TelemetryEvents = new TelemetryEvents(this.STUDY_TELEMETRY_CATEGORY);

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
        this.endStudy("study_disable");
      }
    }
  }

  startupStudy() {
    // enable before any possible calls to endStudy, since it sends an 'end_study' event
    this.TelemetryEvents.enableCollection();

    if (!this.isEligible()) {
      this.endStudy("ineligible");
      return;
    }

    this.initStudyDuration();

    if (this.isStudyExpired()) {
      log.debug("Study expired in between this and the previous session.");
      this.endStudy("expired");
    }
  }

  isEligible() {
    const isAlwaysPrivateBrowsing = Services.prefs.getBoolPref(this.ALWAYS_PRIVATE_BROWSING_PREF);
    if (isAlwaysPrivateBrowsing) {
      return false;
    }

    return true;
  }

  initStudyDuration() {
    if (Services.prefs.getStringPref(this.STUDY_EXPIRATION_DATE_PREF, "")) {
      return;
    }
    Services.prefs.setStringPref(
      this.STUDY_EXPIRATION_DATE_PREF,
      this.getExpirationDateString()
    );
  }

  getDurationFromPref() {
    return Services.prefs.getIntPref(this.STUDY_DURATION_OVERRIDE_PREF, 0);
  }

  getExpirationDateString() {
    const now = Date.now();
    const studyDurationInMs =
    this.getDurationFromPref()
      || this.DEFAULT_STUDY_DURATION_MS;
    const expirationDateInt = now + studyDurationInMs;
    return new Date(expirationDateInt).toISOString();
  }

  isStudyExpired() {
    const expirationDateInt =
      Date.parse(Services.prefs.getStringPref(
        this.STUDY_EXPIRATION_DATE_PREF,
        this.getExpirationDateString()
      ));

    if (isNaN(expirationDateInt)) {
      log.error(
        `The value for the preference ${this.STUDY_EXPIRATION_DATE_PREF} is invalid.`
      );
      return false;
    }

    if (Date.now() > expirationDateInt) {
      return true;
    }
    return false;
  }

  endStudy(reason) {
    log.debug(`Ending the study due to reason: ${ reason }`);
    const isStudyEnding = true;
    Services.telemetry.recordEvent(this.STUDY_TELEMETRY_CATEGORY, "end_study", reason, null,
                                  { subcategory: "shield" });
    this.TelemetryEvents.disableCollection();
    this.uninit(isStudyEnding);
    // These prefs needs to persist between restarts, so only reset on endStudy
    Services.prefs.clearUserPref(this.STUDY_PREF);
    Services.prefs.clearUserPref(this.STUDY_EXPIRATION_DATE_PREF);
  }

  // Called on every Firefox shutdown and endStudy
  uninit(isStudyEnding = false) {
    // if just shutting down, check for expiration, so the endStudy event can
    // be sent along with this session's main ping.
    if (!isStudyEnding && this.isStudyExpired()) {
      log.debug("Study expired during this session.");
      this.endStudy("expired");
      return;
    }

    Services.prefs.removeObserver(this.ALWAYS_PRIVATE_BROWSING_PREF, this);
    Services.prefs.removeObserver(this.STUDY_PREF, this);
    Services.prefs.removeObserver(this.STUDY_DURATION_OVERRIDE_PREF, this);
    Services.prefs.clearUserPref(PREF_LOG_LEVEL);
    Services.prefs.clearUserPref(this.STUDY_DURATION_OVERRIDE_PREF);
  }
}

// References:
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

const SavantShieldStudy = new SavantShieldStudyClass();
