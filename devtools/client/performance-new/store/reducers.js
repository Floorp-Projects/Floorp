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
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
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

      const { isActive } = action;
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
          // Do nothing.
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

/**
 * This object represents the default recording settings. They should be
 * overriden by whatever is read from the Firefox preferences at load time.
 * @type {RecordingSettings}
 */
const DEFAULT_RECORDING_SETTINGS = {
  // The preset name.
  presetName: "",
  // The setting for the recording interval. Defaults to 1ms.
  interval: 1,
  // The number of entries in the profiler's circular buffer.
  entries: 0,
  // The features that are enabled for the profiler.
  features: [],
  // The thread list
  threads: [],
  // The objdirs list
  objdirs: [],
  // The client doesn't implement durations yet. See Bug 1587165.
  duration: 0,
};

/**
 * This small utility returns true if the parameters contain the same values.
 * This is essentially a deepEqual operation specific to this structure.
 * @param {RecordingSettings} a
 * @param {RecordingSettings} b
 * @return {boolean}
 */
function areSettingsEquals(a, b) {
  if (a === b) {
    return true;
  }

  /* Simple properties */
  /* These types look redundant, but they actually help TypeScript assess that
   * the following code is correct, as well as prevent typos. */
  /** @type {Array<"presetName" | "interval" | "entries" | "duration">} */
  const simpleProperties = ["presetName", "interval", "entries", "duration"];

  /* arrays */
  /** @type {Array<"features" | "threads" | "objdirs">} */
  const arrayProperties = ["features", "threads", "objdirs"];

  for (const property of simpleProperties) {
    if (a[property] !== b[property]) {
      return false;
    }
  }

  for (const property of arrayProperties) {
    if (a[property].length !== b[property].length) {
      return false;
    }

    const arrayA = a[property].slice().sort();
    const arrayB = b[property].slice().sort();
    if (arrayA.some((valueA, i) => valueA !== arrayB[i])) {
      return false;
    }
  }

  return true;
}

/**
 * This handles all values used as recording settings.
 * @type {Reducer<RecordingSettings>}
 */
function recordingSettings(state = DEFAULT_RECORDING_SETTINGS, action) {
  /**
   * @template {keyof RecordingSettings} K
   * @param {K} settingName
   * @param {RecordingSettings[K]} settingValue
   * @return {RecordingSettings}
   */
  function changeOneSetting(settingName, settingValue) {
    if (state[settingName] === settingValue) {
      // Do not change the state if the new value equals the old value.
      return state;
    }

    return {
      ...state,
      [settingName]: settingValue,
      presetName: "custom",
    };
  }

  switch (action.type) {
    case "CHANGE_INTERVAL":
      return changeOneSetting("interval", action.interval);
    case "CHANGE_ENTRIES":
      return changeOneSetting("entries", action.entries);
    case "CHANGE_FEATURES":
      return changeOneSetting("features", action.features);
    case "CHANGE_THREADS":
      return changeOneSetting("threads", action.threads);
    case "CHANGE_OBJDIRS":
      return changeOneSetting("objdirs", action.objdirs);
    case "CHANGE_PRESET":
      return action.preset
        ? {
            ...state,
            presetName: action.presetName,
            interval: action.preset.interval,
            entries: action.preset.entries,
            features: action.preset.features,
            threads: action.preset.threads,
            // The client doesn't implement durations yet. See Bug 1587165.
            duration: action.preset.duration,
          }
        : {
            ...state,
            presetName: action.presetName, // it's probably "custom".
          };
    case "UPDATE_SETTINGS_FROM_PREFERENCES":
      if (areSettingsEquals(state, action.recordingSettingsFromPreferences)) {
        return state;
      }
      return { ...action.recordingSettingsFromPreferences };
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
    recordingSettings: recordingSettings(state?.recordingSettings, action),
    initializedValues: initializedValues(state?.initializedValues, action),
    promptEnvRestart: promptEnvRestart(state?.promptEnvRestart, action),
  };
};
