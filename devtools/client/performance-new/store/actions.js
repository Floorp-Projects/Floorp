/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

const selectors = require("devtools/client/performance-new/store/selectors");
const {
  translatePreferencesToState,
  translatePreferencesFromState,
} = require("devtools/client/performance-new/preference-management");
const {
  getEnvironmentVariable,
} = require("devtools/client/performance-new/browser");

/**
 * @typedef {import("../@types/perf").Action} Action
 * @typedef {import("../@types/perf").Library} Library
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").InitializeStoreValues} InitializeStoreValues
 * @typedef {import("../@types/perf").Presets} Presets
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 */

/**
 * @template T
 * @typedef {import("../@types/perf").ThunkAction<T>} ThunkAction<T>
 */

/**
 * The recording state manages the current state of the recording panel.
 * @param {RecordingState} state - A valid state in `recordingState`.
 * @param {{ didRecordingUnexpectedlyStopped: boolean }} options
 * @return {Action}
 */
const changeRecordingState = (exports.changeRecordingState = (
  state,
  options = { didRecordingUnexpectedlyStopped: false }
) => ({
  type: "CHANGE_RECORDING_STATE",
  state,
  didRecordingUnexpectedlyStopped: options.didRecordingUnexpectedlyStopped,
}));

/**
 * This is the result of the initial questions about the state of the profiler.
 *
 * @param {boolean} isSupportedPlatform - This is a supported platform.
 * @param {RecordingState} recordingState - A valid state in `recordingState`.
 * @return {Action}
 */
exports.reportProfilerReady = (isSupportedPlatform, recordingState) => ({
  type: "REPORT_PROFILER_READY",
  isSupportedPlatform,
  recordingState,
});

/**
 * Dispatch the given action, and then update the recording settings.
 * @param {Action} action
 * @return {ThunkAction<void>}
 */
function _dispatchAndUpdatePreferences(action) {
  return (dispatch, getState) => {
    if (typeof action !== "object") {
      throw new Error(
        "This function assumes that the dispatched action is a simple object and " +
          "synchronous."
      );
    }
    dispatch(action);
    const setRecordingPreferences = selectors.getSetRecordingPreferencesFn(
      getState()
    );
    const recordingSettings = selectors.getRecordingSettings(getState());
    setRecordingPreferences(translatePreferencesFromState(recordingSettings));
  };
}

/**
 * Updates the recording settings for the interval.
 * @param {number} interval
 * @return {ThunkAction<void>}
 */
exports.changeInterval = interval =>
  _dispatchAndUpdatePreferences({
    type: "CHANGE_INTERVAL",
    interval,
  });

/**
 * Updates the recording settings for the entries.
 * @param {number} entries
 * @return {ThunkAction<void>}
 */
exports.changeEntries = entries =>
  _dispatchAndUpdatePreferences({
    type: "CHANGE_ENTRIES",
    entries,
  });

/**
 * Updates the recording settings for the features.
 * @param {string[]} features
 * @return {ThunkAction<void>}
 */
exports.changeFeatures = features => {
  return (dispatch, getState) => {
    let promptEnvRestart = null;
    if (selectors.getPageContext(getState()) === "aboutprofiling") {
      // TODO Bug 1615431 - The popup supported restarting the browser, but
      // this hasn't been updated yet for the about:profiling workflow.
      if (
        !getEnvironmentVariable("JS_TRACE_LOGGING") &&
        features.includes("jstracer")
      ) {
        promptEnvRestart = "JS_TRACE_LOGGING";
      }
    }

    dispatch(
      _dispatchAndUpdatePreferences({
        type: "CHANGE_FEATURES",
        features,
        promptEnvRestart,
      })
    );
  };
};

/**
 * Updates the recording settings for the threads.
 * @param {string[]} threads
 * @return {ThunkAction<void>}
 */
exports.changeThreads = threads =>
  _dispatchAndUpdatePreferences({
    type: "CHANGE_THREADS",
    threads,
  });

/**
 * Change the preset.
 * @param {Presets} presets
 * @param {string} presetName
 * @return {ThunkAction<void>}
 */
exports.changePreset = (presets, presetName) =>
  _dispatchAndUpdatePreferences({
    type: "CHANGE_PRESET",
    presetName,
    // Also dispatch the preset so that the reducers can pre-fill the values
    // from a preset.
    preset: presets[presetName],
  });

/**
 * Updates the recording settings for the objdirs.
 * @param {string[]} objdirs
 * @return {ThunkAction<void>}
 */
exports.changeObjdirs = objdirs =>
  _dispatchAndUpdatePreferences({
    type: "CHANGE_OBJDIRS",
    objdirs,
  });

/**
 * Receive the values to initialize the store. See the reducer for what values
 * are expected.
 * @param {InitializeStoreValues} values
 * @return {Action}
 */
exports.initializeStore = values => {
  const { recordingPreferences, ...initValues } = values;
  return {
    ...initValues,
    type: "INITIALIZE_STORE",
    recordingSettingsFromPreferences: translatePreferencesToState(
      recordingPreferences
    ),
  };
};

/**
 * Start a new recording with the perfFront and update the internal recording state.
 * @return {ThunkAction<void>}
 */
exports.startRecording = () => {
  return (dispatch, getState) => {
    const recordingSettings = selectors.getRecordingSettings(getState());
    const perfFront = selectors.getPerfFront(getState());
    // In the case of the profiler popup, the startProfiler can be synchronous.
    // In order to properly allow the React components to handle the state changes
    // make sure and change the recording state first, then start the profiler.
    dispatch(changeRecordingState("request-to-start-recording"));
    perfFront.startProfiler(recordingSettings);
  };
};

/**
 * Stops the profiler, and opens the profile in a new window.
 * @return {ThunkAction<void>}
 */
exports.getProfileAndStopProfiler = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState("request-to-get-profile-and-stop-profiler"));
    const profile = await perfFront.getProfileAndStopProfiler();

    const getSymbolTable = selectors.getSymbolTableGetter(getState())(profile);
    const receiveProfile = selectors.getReceiveProfileFn(getState());
    receiveProfile(profile, getSymbolTable);
    dispatch(changeRecordingState("available-to-record"));
  };
};

/**
 * Stops the profiler, but does not try to retrieve the profile.
 * @return {ThunkAction<void>}
 */
exports.stopProfilerAndDiscardProfile = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState("request-to-stop-profiler"));

    try {
      await perfFront.stopProfilerAndDiscardProfile();
    } catch (error) {
      /** @type {any} */
      const anyWindow = window;
      /** @type {PanelWindow} - Coerce the window into the PanelWindow. */
      const { gIsPanelDestroyed } = anyWindow;

      if (gIsPanelDestroyed) {
        // This error is most likely "Connection closed, pending request" as the
        // command can race with closing the panel. Do not report an error. It's
        // most likely fine.
      } else {
        throw error;
      }
    }
  };
};
