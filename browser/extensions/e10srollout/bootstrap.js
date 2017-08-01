/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm");

// The amount of people to be part of e10s
const TEST_THRESHOLD = {
  "beta": 0.9,  // 90%
  "release": 1.0,  // 100%
  "esr": 1.0,  // 100%
};

// If a user qualifies for the e10s-multi experiement, this is how many
// content processes to use and whether to allow addons for the experiment.
const MULTI_EXPERIMENT = {
  "beta": { buckets: { 1: .5, 4: 1, }, // 1 process: 50%, 4 processes: 50%

            // When on the "beta" channel, getAddonsDisqualifyForMulti
            // will return true if any addon installed is not a web extension.
            // Therefore, this returns true if and only if all addons
            // installed are web extensions or if no addons are installed
            // at all.
            addonsDisableExperiment(prefix) { return getAddonsDisqualifyForMulti(); } },

  "release": { buckets: { 1: .5, 4: 1 }, // 1 process: 50%, 4 processes: 50%

               // See above for an explanation of this: we only allow users
               // with no extensions or users with WebExtensions.
               addonsDisableExperiment(prefix) { return getAddonsDisqualifyForMulti(); } }
};

const ADDON_ROLLOUT_POLICY = {
  "beta": "50allmpc",
  "release": "50allmpc",
  "esr": "esrA", // WebExtensions and Addons with mpc=true
};

if (AppConstants.RELEASE_OR_BETA) {
  // Bug 1348576 - e10s is never enabled for non-official release builds
  // This is hacky, but the problem it solves is the following:
  // the e10s rollout is controlled by the channel name, which
  // is the only way to distinguish between Beta and Release.
  // However, non-official release builds (like the ones done by distros
  // to ship Firefox on their package managers) do not set a value
  // for the release channel, which gets them to the default value
  // of.. (drumroll) "default".
  // But we can't just always configure the same settings for the
  // "default" channel because that's also the name that a locally
  // built Firefox gets, and e10s is managed in a different way
  // there (directly by prefs, on Nightly and Aurora).
  TEST_THRESHOLD.default = TEST_THRESHOLD.release;
  ADDON_ROLLOUT_POLICY.default = ADDON_ROLLOUT_POLICY.release;
}


const PREF_COHORT_SAMPLE       = "e10s.rollout.cohortSample";
const PREF_COHORT_NAME         = "e10s.rollout.cohort";
const PREF_E10S_OPTED_IN       = "browser.tabs.remote.autostart";
const PREF_E10S_FORCE_ENABLED  = "browser.tabs.remote.force-enable";
const PREF_E10S_FORCE_DISABLED = "browser.tabs.remote.force-disable";
const PREF_TOGGLE_E10S         = "browser.tabs.remote.autostart.2";
const PREF_E10S_ADDON_POLICY   = "extensions.e10s.rollout.policy";
const PREF_E10S_ADDON_BLOCKLIST = "extensions.e10s.rollout.blocklist";
const PREF_E10S_HAS_NONEXEMPT_ADDON = "extensions.e10s.rollout.hasAddon";
const PREF_E10S_MULTI_OPTOUT   = "dom.ipc.multiOptOut";
const PREF_E10S_PROCESSCOUNT   = "dom.ipc.processCount";
const PREF_USE_DEFAULT_PERF_SETTINGS = "browser.preferences.defaultPerformanceSettings.enabled";
const PREF_E10S_MULTI_ADDON_BLOCKS = "extensions.e10sMultiBlocksEnabling";
const PREF_E10S_MULTI_BLOCKED_BY_ADDONS = "extensions.e10sMultiBlockedByAddons";

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
    Services.prefs.setStringPref(PREF_E10S_ADDON_POLICY, addonPolicy);

    // This is also the proper place to set the blocklist pref
    // in case it is necessary.
    Services.prefs.setStringPref(PREF_E10S_ADDON_BLOCKLIST, "");
  } else {
    Services.prefs.clearUserPref(PREF_E10S_ADDON_POLICY);
  }

  let userOptedOut = optedOut();
  let userOptedIn = optedIn();
  let disqualified = (Services.appinfo.multiprocessBlockPolicy != 0);
  let testThreshold = TEST_THRESHOLD[updateChannel];
  let testGroup = (getUserSample(false) < testThreshold);
  let hasNonExemptAddon = Services.prefs.getBoolPref(PREF_E10S_HAS_NONEXEMPT_ADDON, false);
  let temporaryDisqualification = getTemporaryDisqualification();
  let temporaryQualification = getTemporaryQualification();

  let cohortPrefix = "";
  if (disqualified) {
    cohortPrefix = "disqualified-";
  } else if (hasNonExemptAddon) {
    cohortPrefix = `addons-set${addonPolicy}-`;
  }

  let eligibleForMulti = false;
  if (userOptedOut.e10s || userOptedOut.multi) {
    // If we detected that the user opted out either for multi or e10s, then
    // the proper prefs must already be set.
    setCohort("optedOut");
  } else if (userOptedIn.e10s) {
    eligibleForMulti = true;
    setCohort("optedIn");
  } else if (temporaryDisqualification != "") {
    // Users who are disqualified by the backend (from multiprocessBlockPolicy)
    // can be put into either the test or control groups, because e10s will
    // still be denied by the backend, which is useful so that the E10S_STATUS
    // telemetry probe can be correctly set.

    // For these volatile disqualification reasons, however, we must not try
    // to activate e10s because the backend doesn't know about it. E10S_STATUS
    // here will be accumulated as "2 - Disabled", which is fine too.
    Services.prefs.clearUserPref(PREF_TOGGLE_E10S);
    Services.prefs.clearUserPref(PREF_E10S_PROCESSCOUNT + ".web");
    setCohort(`temp-disqualified-${temporaryDisqualification}`);
  } else if (!disqualified && testThreshold < 1.0 &&
             temporaryQualification != "") {
    // Users who are qualified for e10s and on channels where some population
    // would not receive e10s can be pushed into e10s anyway via a temporary
    // qualification which overrides the user sample value when non-empty.
    Services.prefs.setBoolPref.set(PREF_TOGGLE_E10S, true);
    eligibleForMulti = true;
    setCohort(`temp-qualified-${temporaryQualification}`);
  } else if (testGroup) {
    Services.prefs.setBoolPref(PREF_TOGGLE_E10S, true);
    eligibleForMulti = true;
    setCohort(`${cohortPrefix}test`);
  } else {
    Services.prefs.clearUserPref(PREF_TOGGLE_E10S);
    Services.prefs.clearUserPref(PREF_E10S_PROCESSCOUNT + ".web");
    setCohort(`${cohortPrefix}control`);
  }

  // Now determine if this user should be in the e10s-multi experiment.
  // - We only run the experiment on channels defined in MULTI_EXPERIMENT.
  // - If this experiment doesn't allow addons and we have a cohort prefix
  //   (i.e. there's at least one addon installed) we stop here.
  // - We decided above whether this user qualifies for the experiment.
  // - If the user already opted into multi, then their prefs are already set
  //   correctly, we're done.
  // - If the user has addons that disqualify them for multi, leave them with
  //   the default number of content processes (1 on beta) but still in the
  //   test cohort.
  if (!(updateChannel in MULTI_EXPERIMENT) ||
      MULTI_EXPERIMENT[updateChannel].addonsDisableExperiment(cohortPrefix) ||
      !eligibleForMulti ||
      userOptedIn.multi ||
      disqualified) {
    Services.prefs.clearUserPref(PREF_E10S_PROCESSCOUNT + ".web");
    return;
  }

  // If we got here with a cohortPrefix, it must be "addons-set50allmpc-",
  // which means that there's at least one add-on installed. If
  // getAddonsDisqualifyForMulti returns false, that means that all installed
  // addons are webextension based, so note that in the cohort name.
  if (cohortPrefix && !getAddonsDisqualifyForMulti()) {
    cohortPrefix = "webextensions-";
  }

  // The user is in the multi experiment!
  // Decide how many content processes to use for this user.
  let buckets = MULTI_EXPERIMENT[updateChannel].buckets;

  let multiUserSample = getUserSample(true);
  for (let sampleName of Object.getOwnPropertyNames(buckets)) {
    if (multiUserSample < buckets[sampleName]) {
      // NB: Coerce sampleName to an integer because this is an integer pref.
      Services.prefs.setIntPref(PREF_E10S_PROCESSCOUNT + ".web", +sampleName);
      setCohort(`${cohortPrefix}multiBucket${sampleName}`);
      break;
    }
  }
}

function shutdown(data, reason) {
}

function uninstall() {
}

function getUserSample(multi) {
  let pref = multi ? (PREF_COHORT_SAMPLE + ".multi") : PREF_COHORT_SAMPLE;
  let prefType = Services.prefs.getPrefType(pref);

  if (prefType == Ci.nsIPrefBranch.PREF_STRING) {
    return parseFloat(Services.prefs.getStringPref(pref), 10);
  }

  let value = 0.0;
  if (prefType == Ci.nsIPrefBranch.PREF_INT) {
    // convert old integer value
    value = Services.prefs.getIntPref(pref) / 100;
  } else {
    value = Math.random();
  }

  Services.prefs.setStringPref(pref, value.toString().substr(0, 8));
  return value;
}

function setCohort(cohortName) {
  Services.prefs.setStringPref(PREF_COHORT_NAME, cohortName);
  if (cohortName != "unsupportedChannel") {
    TelemetryEnvironment.setExperimentActive("e10sCohort", cohortName);
  }
  try {
    if (Ci.nsICrashReporter) {
      Services.appinfo.QueryInterface(Ci.nsICrashReporter).annotateCrashReport("E10SCohort", cohortName);
    }
  } catch (e) {}
}

function optedIn() {
  let e10s = Services.prefs.getBoolPref(PREF_E10S_OPTED_IN, false) ||
             Services.prefs.getBoolPref(PREF_E10S_FORCE_ENABLED, false);
  let multi = Services.prefs.prefHasUserValue(PREF_E10S_PROCESSCOUNT) ||
             !Services.prefs.getBoolPref(PREF_USE_DEFAULT_PERF_SETTINGS, true);
  return { e10s, multi };
}

function optedOut() {
  // Users can also opt-out by toggling back the pref to false.
  // If they reset the pref instead they might be re-enabled if
  // they are still part of the threshold.
  let e10s = Services.prefs.getBoolPref(PREF_E10S_FORCE_DISABLED, false) ||
               (Services.prefs.prefHasUserValue(PREF_TOGGLE_E10S) &&
                Services.prefs.getBoolPref(PREF_TOGGLE_E10S) == false);
  let multi = Services.prefs.getIntPref(PREF_E10S_MULTI_OPTOUT, 0) >=
              Services.appinfo.E10S_MULTI_EXPERIMENT;
  return { e10s, multi };
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

/* If this function returns a non-empty string, it
 * means that this particular user should be temporarily
 * qualified due to some particular reason.
 * If a user shouldn't be qualified, then an empty
 * string must be returned.
 */
function getTemporaryQualification() {
  // Whenever the DevTools toolbox is opened for the first time in a release, it
  // records this fact in the following pref as part of the DevTools telemetry
  // system.  If this pref is set, then it means the user has opened DevTools at
  // some point in time.
  const PREF_OPENED_DEVTOOLS = "devtools.telemetry.tools.opened.version";
  let hasOpenedDevTools = Services.prefs.prefHasUserValue(PREF_OPENED_DEVTOOLS);
  if (hasOpenedDevTools) {
    return "devtools";
  }

  return "";
}

function getAddonsDisqualifyForMulti() {
  return Services.prefs.getBoolPref("extensions.e10sMultiBlocksEnabling", false) &&
         Services.prefs.getBoolPref("extensions.e10sMultiBlockedByAddons", false);
}
