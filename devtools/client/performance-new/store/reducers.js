/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

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
// eslint-disable-next-line complexity
function recordingState(state = "not-yet-known", action) {
  switch (action.type) {
    case "REPORT_PROFILER_READY": {
      // It's theoretically possible we got an event that already let us know about
      // the current state of the profiler.
      if (state !== "not-yet-known") {
        return state;
      }

      const { isActive, isLockedForPrivateBrowsing } = action;
      if (isLockedForPrivateBrowsing) {
        return "locked-by-private-browsing";
      }
      if (isActive) {
        return "recording";
      }
      return "available-to-record";
    }

    case "REPORT_PROFILER_STARTED":
      switch (state) {
        case "not-yet-known":
        // We couldn't have started it yet, so it must have been someone
        // else. (fallthrough)
        case "available-to-record":
        // We aren't recording, someone else started it up. (fallthrough)
        case "request-to-stop-profiler":
        // We requested to stop the profiler, but someone else already started
        // it up. (fallthrough)
        case "request-to-get-profile-and-stop-profiler":
          return "recording";

        case "request-to-start-recording":
          // Wait for the profiler to tell us that it has started.
          return "recording";

        case "locked-by-private-browsing":
        case "recording":
          // These state cases don't make sense to happen, and means we have a logical
          // fallacy somewhere.
          throw new Error(
            "The profiler started recording, when it shouldn't have " +
              `been able to. Current state: "${state}"`
          );
        default:
          throw new Error("Unhandled recording state");
      }

    case "REPORT_PROFILER_STOPPED":
      switch (state) {
        case "not-yet-known":
        case "request-to-get-profile-and-stop-profiler":
        case "request-to-stop-profiler":
          return "available-to-record";

        case "request-to-start-recording":
        // Highly unlikely, but someone stopped the recorder, this is fine.
        // Do nothing (fallthrough).
        case "locked-by-private-browsing":
          // The profiler is already locked, so we know about this already.
          return state;

        case "recording":
          return "available-to-record";

        case "available-to-record":
          throw new Error(
            "The profiler stopped recording, when it shouldn't have been able to."
          );
        default:
          throw new Error("Unhandled recording state");
      }

    case "REPORT_PRIVATE_BROWSING_STARTED":
      switch (state) {
        case "request-to-get-profile-and-stop-profiler":
        // This one is a tricky case. Go ahead and act like nothing went wrong, maybe
        // it will resolve correctly? (fallthrough)
        case "request-to-stop-profiler":
        case "available-to-record":
        case "not-yet-known":
          return "locked-by-private-browsing";

        case "request-to-start-recording":
        case "recording":
          return "locked-by-private-browsing";

        case "locked-by-private-browsing":
          // Do nothing
          return state;

        default:
          throw new Error("Unhandled recording state");
      }

    case "REPORT_PRIVATE_BROWSING_STOPPED":
      // No matter the state, go ahead and set this as ready to record. This should
      // be the only logical state to go into.
      return "available-to-record";

    case "REQUESTING_TO_START_RECORDING":
      return "request-to-start-recording";

    case "REQUESTING_TO_STOP_RECORDING":
      return "request-to-stop-profiler";

    case "REQUESTING_PROFILE":
      return "request-to-get-profile-and-stop-profiler";

    case "OBTAINED_PROFILE":
      return "available-to-record";

    default:
      return state;
  }
}

/**
 * Whether or not the recording state unexpectedly stopped. This allows
 * the UI to display a helpful message.
 * @param {RecordingState | undefined} recState
 * @param {boolean} state
 * @param {Action} action
 * @returns {boolean}
 */
function recordingUnexpectedlyStopped(recState, state = false, action) {
  switch (action.type) {
    case "REPORT_PROFILER_STOPPED":
    case "REPORT_PRIVATE_BROWSING_STARTED":
      if (
        recState === "recording" ||
        recState == "request-to-start-recording"
      ) {
        return true;
      }
      return state;
    case "REPORT_PROFILER_STARTED":
      return false;
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
    case "INITIALIZE_STORE":
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
        setRecordingSettings: action.setRecordingSettings,
        presets: action.presets,
        pageContext: action.pageContext,
        supportedFeatures: action.supportedFeatures,
        openRemoteDevTools: action.openRemoteDevTools,
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
module.exports = (state = undefined, action) => {
  return {
    recordingState: recordingState(state?.recordingState, action),

    // Treat this one specially - it also gets the recordingState.
    recordingUnexpectedlyStopped: recordingUnexpectedlyStopped(
      state?.recordingState,
      state?.recordingUnexpectedlyStopped,
      action
    ),

    isSupportedPlatform: isSupportedPlatform(
      state?.isSupportedPlatform,
      action
    ),
    interval: interval(state?.interval, action),
    entries: entries(state?.entries, action),
    features: features(state?.features, action),
    threads: threads(state?.threads, action),
    objdirs: objdirs(state?.objdirs, action),
    presetName: presetName(state?.presetName, action),
    initializedValues: initializedValues(state?.initializedValues, action),
    promptEnvRestart: promptEnvRestart(state?.promptEnvRestart, action),

    profilerViewMode: state?.profilerViewMode,
  };
};
