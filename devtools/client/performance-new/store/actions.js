/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

const selectors = require("resource://devtools/client/performance-new/store/selectors.js");

/**
 * @typedef {import("../@types/perf").Action} Action
 * @typedef {import("../@types/perf").Library} Library
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").InitializeStoreValues} InitializeStoreValues
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("../@types/perf").Presets} Presets
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 * @typedef {import("../@types/perf").MinimallyTypedGeckoProfile} MinimallyTypedGeckoProfile
 */

/**
 * @template T
 * @typedef {import("../@types/perf").ThunkAction<T>} ThunkAction<T>
 */

/**
 * This is the result of the initial questions about the state of the profiler.
 *
 * @param {boolean} isActive
 * @return {Action}
 */
exports.reportProfilerReady = isActive => ({
  type: "REPORT_PROFILER_READY",
  isActive,
});

/**
 * Dispatched when the profiler starting is observed.
 * @return {Action}
 */
exports.reportProfilerStarted = () => ({
  type: "REPORT_PROFILER_STARTED",
});

/**
 * Dispatched when the profiler stopping is observed.
 * @return {Action}
 */
exports.reportProfilerStopped = () => ({
  type: "REPORT_PROFILER_STOPPED",
});

/**
 * Updates the recording settings for the interval.
 * @param {number} interval
 * @return {Action}
 */
exports.changeInterval = interval => ({
  type: "CHANGE_INTERVAL",
  interval,
});

/**
 * Updates the recording settings for the entries.
 * @param {number} entries
 * @return {Action}
 */
exports.changeEntries = entries => ({
  type: "CHANGE_ENTRIES",
  entries,
});

/**
 * Updates the recording settings for the features.
 * @param {string[]} features
 * @return {ThunkAction<void>}
 */
exports.changeFeatures = features => {
  return ({ dispatch, getState }) => {
    let promptEnvRestart = null;
    if (selectors.getPageContext(getState()) === "aboutprofiling") {
      // TODO Bug 1615431 - The old popup supported restarting the browser, but
      // this hasn't been updated yet for the about:profiling workflow, because
      // jstracer is disabled for now.
      if (
        !Services.env.get("JS_TRACE_LOGGING") &&
        features.includes("jstracer")
      ) {
        promptEnvRestart = "JS_TRACE_LOGGING";
      }
    }

    dispatch({
      type: "CHANGE_FEATURES",
      features,
      promptEnvRestart,
    });
  };
};

/**
 * Updates the recording settings for the threads.
 * @param {string[]} threads
 * @return {Action}
 */
exports.changeThreads = threads => ({
  type: "CHANGE_THREADS",
  threads,
});

/**
 * Change the preset.
 * @param {Presets} presets
 * @param {string} presetName
 * @return {Action}
 */
exports.changePreset = (presets, presetName) => ({
  type: "CHANGE_PRESET",
  presetName,
  // Also dispatch the preset so that the reducers can pre-fill the values
  // from a preset.
  preset: presets[presetName],
});

/**
 * Updates the recording settings for the objdirs.
 * @param {string[]} objdirs
 * @return {Action}
 */
exports.changeObjdirs = objdirs => ({
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
  return {
    type: "INITIALIZE_STORE",
    ...values,
  };
};

/**
 * Whenever the preferences are updated, this action is dispatched to update the
 * redux store.
 * @param {RecordingSettings} recordingSettingsFromPreferences
 * @return {Action}
 */
exports.updateSettingsFromPreferences = recordingSettingsFromPreferences => {
  return {
    type: "UPDATE_SETTINGS_FROM_PREFERENCES",
    recordingSettingsFromPreferences,
  };
};

/**
 * Start a new recording with the perfFront and update the internal recording state.
 * @param {PerfFront} perfFront
 * @return {ThunkAction<void>}
 */
exports.startRecording = perfFront => {
  return ({ dispatch, getState }) => {
    const recordingSettings = selectors.getRecordingSettings(getState());
    // In the case of the profiler popup, the startProfiler can be synchronous.
    // In order to properly allow the React components to handle the state changes
    // make sure and change the recording state first, then start the profiler.
    dispatch({ type: "REQUESTING_TO_START_RECORDING" });
    perfFront.startProfiler(recordingSettings);
  };
};

/**
 * Stops the profiler, and opens the profile in a new window.
 * @param {PerfFront} perfFront
 * @return {ThunkAction<Promise<MinimallyTypedGeckoProfile>>}
 */
exports.getProfileAndStopProfiler = perfFront => {
  return async ({ dispatch }) => {
    dispatch({ type: "REQUESTING_PROFILE" });
    const profile = await perfFront.getProfileAndStopProfiler();
    dispatch({ type: "OBTAINED_PROFILE" });
    return profile;
  };
};

/**
 * Stops the profiler, but does not try to retrieve the profile.
 * @param {PerfFront} perfFront
 * @return {ThunkAction<void>}
 */
exports.stopProfilerAndDiscardProfile = perfFront => {
  return async ({ dispatch }) => {
    dispatch({ type: "REQUESTING_TO_STOP_RECORDING" });

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
