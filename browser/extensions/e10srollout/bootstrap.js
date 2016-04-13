/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");

 // The amount of people to be part of e10s, in %
const TEST_THRESHOLD = {
  "beta"    : 50,
  "release" : 0,
};

const PREF_COHORT_SAMPLE       = "e10s.rollout.cohortSample";
const PREF_COHORT_NAME         = "e10s.rollout.cohort";
const PREF_E10S_OPTED_IN       = "browser.tabs.remote.autostart";
const PREF_E10S_FORCE_ENABLED  = "browser.tabs.remote.force-enable";
const PREF_E10S_FORCE_DISABLED = "browser.tabs.remote.force-disable";
const PREF_TOGGLE_E10S         = "browser.tabs.remote.autostart.2";


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

  let userOptedOut = optedOut();
  let userOptedIn = optedIn();
  let disqualified = (Services.appinfo.multiprocessBlockPolicy != 0);
  let testGroup = (getUserSample() < TEST_THRESHOLD[updateChannel]);

  if (userOptedOut) {
    setCohort("optedOut");
  } else if (userOptedIn) {
    setCohort("optedIn");
  } else if (disqualified) {
    setCohort("disqualified");
  } else if (testGroup) {
    setCohort("test");
    Preferences.set(PREF_TOGGLE_E10S, true);
  } else {
    setCohort("control");
    Preferences.reset(PREF_TOGGLE_E10S);
  }
}

function shutdown(data, reason) {
}

function uninstall() {
}

function getUserSample() {
  let existingVal = Preferences.get(PREF_COHORT_SAMPLE, undefined);
  if (typeof(existingVal) == "number") {
    return existingVal;
  }

  let val = Math.floor(Math.random() * 100);
  Preferences.set(PREF_COHORT_SAMPLE, val);
  return val;
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
