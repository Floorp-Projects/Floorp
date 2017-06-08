/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm");

// The amount of people to be part of the telemetry reporting.
const REPORTING_THRESHOLD = {
  // "default": 1.0, // 100% - self builds, linux distros etc.
  "nightly": 0.1, // 10%
  "beta": 0.1,  // 10%
  "release": 0.1,  // 10%
};

// Preferences this add-on uses.
const kPrefPrefix = "extensions.followonsearch.";
const PREF_COHORT_SAMPLE = `${kPrefPrefix}cohortSample`;
const PREF_LOGGING = `${kPrefPrefix}logging`;
const PREF_CHANNEL_OVERRIDE = `${kPrefPrefix}override`;

const kExtensionID = "followonsearch@mozilla.com";
const kSaveTelemetryMsg = `${kExtensionID}:save-telemetry`;
const kShutdownMsg = `${kExtensionID}:shutdown`;

const frameScript = `chrome://followonsearch/content/followonsearch-fs.js?q=${Math.random()}`;

const validSearchTypes = [
  // A search is a follow-on search from an SAP.
  "follow-on",
  // The search is a "search access point".
  "sap",
];

var gLoggingEnabled = false;
var gTelemetryActivated = false;

/**
 * Logs a message to the console if logging is enabled.
 *
 * @param {String} message The message to log.
 */
function log(message) {
  if (gLoggingEnabled) {
    console.log("Follow-On Search", message);
  }
}

/**
 * Handles receiving a message from the content process to save telemetry.
 *
 * @param {Object} message The message received.
 */
function handleSaveTelemetryMsg(message) {
  if (message.name != kSaveTelemetryMsg) {
    throw new Error(`Unexpected message received: ${kSaveTelemetryMsg}`);
  }

  let info = message.data;

  if (!validSearchTypes.includes(info.type)) {
    throw new Error("Unexpected type!");
  }

  log(info);

  let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  histogram.add(`${info.sap}.${info.type}:unknown:${info.code}`);
}

/**
 * Activites recording of telemetry if it isn't already activated.
 */
function activateTelemetry() {
  if (gTelemetryActivated) {
    return;
  }

  gTelemetryActivated = true;

  Services.mm.addMessageListener(kSaveTelemetryMsg, handleSaveTelemetryMsg);
  Services.mm.loadFrameScript(frameScript, true);

  // Record the fact we're saving the extra data as a telemetry environment
  // value.
  TelemetryEnvironment.setExperimentActive(kExtensionID, "active");
}

/**
 * Deactivites recording of telemetry if it isn't already deactivated.
 */
function deactivateTelemetry() {
  if (!gTelemetryActivated) {
    return;
  }

  TelemetryEnvironment.setExperimentInactive(kExtensionID);

  Services.mm.removeMessageListener(kSaveTelemetryMsg, handleSaveTelemetryMsg);
  Services.mm.removeDelayedFrameScript(frameScript);
  Services.mm.broadcastAsyncMessage(kShutdownMsg);

  gTelemetryActivated = false;
}

/**
 * cohortManager is used to decide which users to enable the add-on for.
 */
var cohortManager = {
  // Indicates whether the telemetry should be enabled.
  enableForUser: false,

  // Records if we've already run init.
  _definedThisSession: false,

  /**
   * Initialises the manager, working out if telemetry should be enabled
   * for the user.
   */
  init() {
    if (this._definedThisSession) {
      return;
    }

    this._definedThisSession = true;
    this.enableForUser = false;

    try {
      let distId = Services.prefs.getCharPref("distribution.id", "");
      if (distId) {
        log("It is a distribution, not setting up nor enabling.");
        return;
      }
    } catch (e) {}

    let cohortSample;
    try {
      cohortSample = Services.prefs.getFloatPref(PREF_COHORT_SAMPLE, undefined);
    } catch (e) {}
    if (!cohortSample) {
      cohortSample = Math.random().toString().substr(0, 8);
      cohortSample = Services.prefs.setCharPref(PREF_COHORT_SAMPLE, cohortSample);
    }
    log(`Cohort Sample value is ${cohortSample}`);

    let updateChannel = UpdateUtils.getUpdateChannel(false);
    log(`Update channel is ${updateChannel}`);
    if (!(updateChannel in REPORTING_THRESHOLD)) {
      let prefOverride = "default";
      try {
        prefOverride = Services.prefs.getCharPref(PREF_CHANNEL_OVERRIDE, "default");
      } catch (e) {}
      if (prefOverride in REPORTING_THRESHOLD) {
        updateChannel = prefOverride;
      } else {
        // Don't enable, we don't know about the channel, and it isn't overriden.
        return;
      }
    }

    log("Not enabling extra telemetry due to bug 1371198");

    // if (cohortSample <= REPORTING_THRESHOLD[updateChannel]) {
    //   log("Enabling telemetry for user");
    //   this.enableForUser = true;
    // } else {
    //   log("Not enabling telemetry for user - outside threshold.");
    // }
  },
};

/**
 * Called when the add-on is installed.
 *
 * @param {Object} data Data about the add-on.
 * @param {Number} reason Indicates why the extension is being installed.
 */
function install(data, reason) {
  try {
    gLoggingEnabled = Services.prefs.getBoolPref(PREF_LOGGING, false);
  } catch (e) {
    // Needed until Firefox 54
  }

  cohortManager.init();
  if (cohortManager.enableForUser) {
    activateTelemetry();
  }
}

/**
 * Called when the add-on is uninstalled.
 *
 * @param {Object} data Data about the add-on.
 * @param {Number} reason Indicates why the extension is being uninstalled.
 */
function uninstall(data, reason) {
  deactivateTelemetry();
}

/**
 * Called when the add-on starts up.
 *
 * @param {Object} data Data about the add-on.
 * @param {Number} reason Indicates why the extension is being started.
 */
function startup(data, reason) {
  try {
    gLoggingEnabled = Services.prefs.getBoolPref(PREF_LOGGING, false);
  } catch (e) {
    // Needed until Firefox 54
  }

  cohortManager.init();

  if (cohortManager.enableForUser) {
    // Workaround for bug 1202125
    // We need to delay our loading so that when we are upgraded,
    // our new script doesn't get the shutdown message.
    setTimeout(() => {
      activateTelemetry();
    }, 1000);
  }
}

/**
 * Called when the add-on shuts down.
 *
 * @param {Object} data Data about the add-on.
 * @param {Number} reason Indicates why the extension is being shut down.
 */
function shutdown(data, reason) {
  deactivateTelemetry();
}
