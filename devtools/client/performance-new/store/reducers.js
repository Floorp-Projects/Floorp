/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { combineReducers } = require("devtools/client/shared/vendor/redux");

const { recordingState: {
  NOT_YET_KNOWN,
}} = require("devtools/client/performance-new/utils");

/**
 * The current state of the recording.
 * @param state - A recordingState key.
 */
function recordingState(state = NOT_YET_KNOWN, action) {
  switch (action.type) {
    case "CHANGE_RECORDING_STATE":
      return action.state;
    case "REPORT_PROFILER_READY":
      return action.recordingState;
    default:
      return state;
  }
}

/**
 * Whether or not the recording state unexpectedly stopped. This allows
 * the UI to display a helpful message.
 * @param {boolean} state
 */
function recordingUnexpectedlyStopped(state = false, action) {
  switch (action.type) {
    case "CHANGE_RECORDING_STATE":
      return action.didRecordingUnexpectedlyStopped;
    default:
      return state;
  }
}

/**
 * The profiler needs to be queried asynchronously on whether or not
 * it supports the user's platform.
 * @param {boolean | null} state
 */
function isSupportedPlatform(state = null, action) {
  switch (action.type) {
    case "REPORT_PROFILER_READY":
      return action.isSupportedPlatform;
    default:
      return state;
  }
}

// Right now all recording setting the defaults are reset every time the panel
// is opened. These should be persisted between sessions. See Bug 1453014.

/**
 * The setting for the recording interval.
 * @param {number} state
 */
function interval(state = 1, action) {
  switch (action.type) {
    case "CHANGE_INTERVAL":
      return action.interval;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.interval;
    default:
      return state;
  }
}

/**
 * The number of entries in the profiler's circular buffer. Defaults to 90mb.
 * @param {number} state
 */
function entries(state = 10000000, action) {
  switch (action.type) {
    case "CHANGE_ENTRIES":
      return action.entries;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.entries;
    default:
      return state;
  }
}

/**
 * The features that are enabled for the profiler.
 * @param {array} state
 */
function features(state = ["js", "stackwalk", "responsiveness"], action) {
  switch (action.type) {
    case "CHANGE_FEATURES":
      return action.features;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.features;
    default:
      return state;
  }
}

/**
 * The current threads list.
 * @param {array of strings} state
 */
function threads(state = ["GeckoMain", "Compositor"], action) {
  switch (action.type) {
    case "CHANGE_THREADS":
      return action.threads;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.threads;
    default:
      return state;
  }
}

/**
 * These are all the values used to initialize the profiler. They should never change
 * once added to the store.
 *
 * state = {
 *   toolbox - The current toolbox.
 *   perfFront - The current Front to the Perf actor.
 *   receiveProfile - A function to receive the profile and open it into a new window.
 *   setRecordingPreferences - A function to set the recording settings.
 * }
 */
function initializedValues(state = null, action) {
  switch (action.type) {
    case "INITIALIZE_STORE":
      return {
        toolbox: action.toolbox,
        perfFront: action.perfFront,
        receiveProfile: action.receiveProfile,
        setRecordingPreferences: action.setRecordingPreferences
      };
    default:
      return state;
  }
}

module.exports = combineReducers({
  recordingState,
  recordingUnexpectedlyStopped,
  isSupportedPlatform,
  interval,
  entries,
  features,
  threads,
  initializedValues,
});
