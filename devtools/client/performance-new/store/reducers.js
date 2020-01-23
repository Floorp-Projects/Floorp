/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");

/**
 * @typedef {import("../@types/perf").Action} Action
 * @typedef {import("../@types/perf").State} State
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").InitializedValues} InitializedValues
 */

/**
 * @template S
 * @typedef {import("../@types/perf").Reducer<S>} Reducer<S>
 */

/**
 * The current state of the recording.
 * @type {Reducer<RecordingState>}
 */
function recordingState(state = "not-yet-known", action) {
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
 * @type {Reducer<boolean>}
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
 * @type {Reducer<boolean | null>}
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
 * The setting for the recording interval. Defaults to 1ms.
 * @type {Reducer<number>}
 */
function interval(state = 1, action) {
  switch (action.type) {
    case "CHANGE_INTERVAL":
      return action.interval;
    case "CHANGE_PRESET":
      return action.preset ? action.preset.interval : state;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.interval;
    default:
      return state;
  }
}

/**
 * The number of entries in the profiler's circular buffer.
 * @type {Reducer<number>}
 */
function entries(state = 0, action) {
  switch (action.type) {
    case "CHANGE_ENTRIES":
      return action.entries;
    case "CHANGE_PRESET":
      return action.preset ? action.preset.entries : state;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.entries;
    default:
      return state;
  }
}

/**
 * The features that are enabled for the profiler.
 * @type {Reducer<string[]>}
 */
function features(state = [], action) {
  switch (action.type) {
    case "CHANGE_FEATURES":
      return action.features;
    case "CHANGE_PRESET":
      return action.preset ? action.preset.features : state;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.features;
    default:
      return state;
  }
}

/**
 * The current threads list.
 * @type {Reducer<string[]>}
 */
function threads(state = [], action) {
  switch (action.type) {
    case "CHANGE_THREADS":
      return action.threads;
    case "CHANGE_PRESET":
      return action.preset ? action.preset.threads : state;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.threads;
    default:
      return state;
  }
}

/**
 * The current objdirs list.
 * @type {Reducer<string[]>}
 */
function objdirs(state = [], action) {
  switch (action.type) {
    case "CHANGE_OBJDIRS":
      return action.objdirs;
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.objdirs;
    default:
      return state;
  }
}

/**
 * The current preset name, used to select
 * @type {Reducer<string>}
 */
function presetName(state = "", action) {
  switch (action.type) {
    case "INITIALIZE_STORE":
      return action.recordingSettingsFromPreferences.presetName;
    case "CHANGE_PRESET":
      return action.presetName;
    case "CHANGE_INTERVAL":
    case "CHANGE_ENTRIES":
    case "CHANGE_FEATURES":
    case "CHANGE_THREADS":
      // When updating any values, switch the preset over to "custom".
      return "custom";
    default:
      return state;
  }
}

/**
 * These are all the values used to initialize the profiler. They should never
 * change once added to the store.
 *
 * @type {Reducer<InitializedValues | null>}
 */
function initializedValues(state = null, action) {
  switch (action.type) {
    case "INITIALIZE_STORE":
      return {
        perfFront: action.perfFront,
        receiveProfile: action.receiveProfile,
        setRecordingPreferences: action.setRecordingPreferences,
        pageContext: action.pageContext,
        getSymbolTableGetter: action.getSymbolTableGetter,
        supportedFeatures: action.supportedFeatures,
      };
    default:
      return state;
  }
}

/**
 * Some features may need a browser restart with an environment flag. Request
 * one here.
 *
 * @type {Reducer<string | null>}
 */
function promptEnvRestart(state = null, action) {
  switch (action.type) {
    case "CHANGE_FEATURES":
      return action.promptEnvRestart;
    default:
      return state;
  }
}

/**
 * The main reducer for the performance-new client.
 * @type {Reducer<State>}
 */
module.exports = combineReducers({
  // TODO - The object going into `combineReducers` is not currently type checked
  // as being correct for. For instance, recordingState here could be removed, or
  // not return the right state, and TypeScript will not create an error.
  recordingState,
  recordingUnexpectedlyStopped,
  isSupportedPlatform,
  interval,
  entries,
  features,
  threads,
  objdirs,
  presetName,
  initializedValues,
  promptEnvRestart,
});
