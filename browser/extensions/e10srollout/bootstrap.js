/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");

 // The amount of people to be part of e10s
const TEST_THRESHOLD = {
  "beta"    : 0.5,  // 50%
  "release" : 1.0,  // 100%
};

const ADDON_ROLLOUT_POLICY = {
  "beta"    : "51alladdons", // Any WebExtension or addon except with mpc = false
  "release" : "51set1",
};

const PREF_COHORT_SAMPLE       = "e10s.rollout.cohortSample";
const PREF_COHORT_NAME         = "e10s.rollout.cohort";
const PREF_E10S_OPTED_IN       = "browser.tabs.remote.autostart";
const PREF_E10S_FORCE_ENABLED  = "browser.tabs.remote.force-enable";
const PREF_E10S_FORCE_DISABLED = "browser.tabs.remote.force-disable";
const PREF_TOGGLE_E10S         = "browser.tabs.remote.autostart.2";
const PREF_E10S_ADDON_POLICY   = "extensions.e10s.rollout.policy";
const PREF_E10S_ADDON_BLOCKLIST = "extensions.e10s.rollout.blocklist";
const PREF_E10S_HAS_NONEXEMPT_ADDON = "extensions.e10s.rollout.hasAddon";

function startup() {
  // In theory we only need to run this once (on install()), but
  // it's better to also run it on every startup. If the user has
  // made manual changes to the prefs, this will keep the data
  // reported more accurate.
  // It's also fine (and preferred) to just do it here on startup
  // (instead of observing prefs), because e10s takes a restart
  // to take effect, so we keep the data based on how it was when
  // the session started.
  defineCohort();
}

function install() {
  defineCohort();
}

let cohortDefinedOnThisSession = false;

function defineCohort() {
  // Avoid running twice when it was called by install() first
  if (cohortDefinedOnThisSession) {
    return;
  }
  cohortDefinedOnThisSession = true;

  let updateChannel = UpdateUtils.getUpdateChannel(false);
  if (!(updateChannel in TEST_THRESHOLD)) {
    setCohort("unsupportedChannel");
    return;
  }

  let addonPolicy = "unknown";
  if (updateChannel in ADDON_ROLLOUT_POLICY) {
    addonPolicy = ADDON_ROLLOUT_POLICY[updateChannel];
    Preferences.set(PREF_E10S_ADDON_POLICY, addonPolicy);
    // This is also the proper place to set the blocklist pref
    // in case it is necessary.

    // Tab Mix Plus exception tracked at bug 1185672.
    Preferences.set(PREF_E10S_ADDON_BLOCKLIST,
                    "{dc572301-7619-498c-a57d-39143191b318}");
  } else {
    Preferences.reset(PREF_E10S_ADDON_POLICY);
  }

  let userOptedOut = optedOut();
  let userOptedIn = optedIn();
  let disqualified = (Services.appinfo.multiprocessBlockPolicy != 0);
  let testGroup = (getUserSample() < TEST_THRESHOLD[updateChannel]);
  let hasNonExemptAddon = Preferences.get(PREF_E10S_HAS_NONEXEMPT_ADDON, false);
  let temporaryDisqualification = getTemporaryDisqualification();

  let cohortPrefix = "";
  if (disqualified) {
    cohortPrefix = "disqualified-";
  } else if (hasNonExemptAddon) {
    cohortPrefix = `addons-set${addonPolicy}-`;
  }

  if (userOptedOut) {
    setCohort("optedOut");
  } else if (userOptedIn) {
    setCohort("optedIn");
  } else if (temporaryDisqualification != "") {
    // Users who are disqualified by the backend (from multiprocessBlockPolicy)
    // can be put into either the test or control groups, because e10s will
    // still be denied by the backend, which is useful so that the E10S_STATUS
    // telemetry probe can be correctly set.

    // For these volatile disqualification reasons, however, we must not try
    // to activate e10s because the backend doesn't know about it. E10S_STATUS
    // here will be accumulated as "2 - Disabled", which is fine too.
    setCohort(`temp-disqualified-${temporaryDisqualification}`);
    Preferences.reset(PREF_TOGGLE_E10S);
  } else if (testGroup) {
    setCohort(`${cohortPrefix}test`);
    Preferences.set(PREF_TOGGLE_E10S, true);
  } else {
    setCohort(`${cohortPrefix}control`);
    Preferences.reset(PREF_TOGGLE_E10S);
  }
}

function shutdown(data, reason) {
}

function uninstall() {
}

function getUserSample() {
  let prefValue = Preferences.get(PREF_COHORT_SAMPLE, undefined);
  let value = 0.0;

  if (typeof(prefValue) == "string") {
    value = parseFloat(prefValue, 10);
    return value;
  }

  if (typeof(prefValue) == "number") {
    // convert old integer value
    value = prefValue / 100;
  } else {
    value = Math.random();
  }

  Preferences.set(PREF_COHORT_SAMPLE, value.toString().substr(0, 8));
  return value;
}

function setCohort(cohortName) {
  Preferences.set(PREF_COHORT_NAME, cohortName);
  try {
    if (Ci.nsICrashReporter) {
      Services.appinfo.QueryInterface(Ci.nsICrashReporter).annotateCrashReport("E10SCohort", cohortName);
    }
  } catch (e) {}
}

function optedIn() {
  return Preferences.get(PREF_E10S_OPTED_IN, false) ||
         Preferences.get(PREF_E10S_FORCE_ENABLED, false);
}

function optedOut() {
  // Users can also opt-out by toggling back the pref to false.
  // If they reset the pref instead they might be re-enabled if
  // they are still part of the threshold.
  return Preferences.get(PREF_E10S_FORCE_DISABLED, false) ||
         (Preferences.isSet(PREF_TOGGLE_E10S) &&
          Preferences.get(PREF_TOGGLE_E10S) == false);
}

/* If this function returns a non-empty string, it
 * means that this particular user should be temporarily
 * disqualified due to some particular reason.
 * If a user shouldn't be disqualified, then an empty
 * string must be returned.
 */
function getTemporaryDisqualification() {
  return "";
}
