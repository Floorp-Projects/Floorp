/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm");

// The amount of people to be part of the rollout
const TEST_THRESHOLD = {
  "beta": 0.5,  // 50%
  "release": 0.05,  // 5%
};

if (AppConstants.RELEASE_OR_BETA) {
  // The rollout is controlled by the channel name, which
  // is the only way to distinguish between Beta and Release.
  // However, non-official release builds (like the ones done by distros
  // to ship Firefox on their package managers) do not set a value
  // for the release channel, which gets them to the default value
  // of.. (drumroll) "default".
  // But we can't just always configure the same settings for the
  // "default" channel because that's also the name that a locally
  // built Firefox gets, and CTP is already directly set there
  // through an #ifdef in firefox.js
  TEST_THRESHOLD.default = TEST_THRESHOLD.release;
}

const PREF_COHORT_SAMPLE       = "plugins.ctprollout.cohortSample";
const PREF_COHORT_NAME         = "plugins.ctprollout.cohort";
const PREF_FLASH_STATE         = "plugin.state.flash";

function startup() {
  defineCohort();
}

function defineCohort() {
  let updateChannel = UpdateUtils.getUpdateChannel(false);
  if (!(updateChannel in TEST_THRESHOLD)) {
    return;
  }

  let cohort = Services.prefs.getStringPref(PREF_COHORT_NAME);

  if (!cohort) {
    // The cohort has not been defined yet: this is the first
    // time that we're running. Let's see if the user has
    // a non-default setting to avoid changing it.
    let currentPluginState = Services.prefs.getIntPref(PREF_FLASH_STATE);
    switch (currentPluginState) {
      case Ci.nsIPluginTag.STATE_CLICKTOPLAY:
        cohort = "early-adopter-ctp";
        break;

      case Ci.nsIPluginTag.STATE_DISABLED:
        cohort = "early-adopter-disabled";
        break;

      default:
        // intentionally missing from the list is STATE_ENABLED,
        // which will keep cohort undefined.
        break;
    }
  }

  switch (cohort) {
    case undefined:
    case "test":
    case "control":
    {
      // If it's either test/control, the cohort might have changed
      // if the desired sampling has been changed.
      let testThreshold = TEST_THRESHOLD[updateChannel];
      let userSample = getUserSample();

      if (userSample < testThreshold) {
        cohort = "test";
        let defaultPrefs = Services.prefs.getDefaultBranch("");
        defaultPrefs.setIntPref(PREF_FLASH_STATE, Ci.nsIPluginTag.STATE_CLICKTOPLAY);
      } else if (userSample >= 1.0 - testThreshold) {
        cohort = "control";
      } else {
        cohort = "excluded";
      }

      setCohort(cohort);
      watchForPrefChanges();
      break;
    }

    case "early-adopter-ctp":
    case "early-adopter-disabled":
    default:
      // "user-changed-from-*" will fall into this default case and
      // not do anything special.
      setCohort(cohort);
      break;
  }
}

function getUserSample() {
  let prefType = Services.prefs.getPrefType(PREF_COHORT_SAMPLE);

  if (prefType == Ci.nsIPrefBranch.PREF_STRING) {
    return parseFloat(Services.prefs.getStringPref(PREF_COHORT_SAMPLE), 10);
  }

  let value = Math.random();
  Services.prefs.setStringPref(PREF_COHORT_SAMPLE, value.toString().substr(0, 8));
  return value;
}

function setCohort(cohortName) {
  Services.prefs.setStringPref(PREF_COHORT_NAME, cohortName);
  TelemetryEnvironment.setExperimentActive("clicktoplay-rollout", cohortName);

  try {
    if (Ci.nsICrashReporter) {
      Services.appinfo.QueryInterface(Ci.nsICrashReporter).annotateCrashReport("CTPCohort", cohortName);
    }
  } catch (e) {}
}

function watchForPrefChanges() {
  Services.prefs.addObserver(PREF_FLASH_STATE, function prefWatcher() {
    let currentCohort = Services.prefs.getStringPref(PREF_COHORT_NAME, "unknown");
    setCohort(`user-changed-from-${currentCohort}`);
    Services.prefs.removeObserver(PREF_FLASH_STATE, prefWatcher);
  });
}

function install() {
}

function shutdown(data, reason) {
}

function uninstall() {
}
