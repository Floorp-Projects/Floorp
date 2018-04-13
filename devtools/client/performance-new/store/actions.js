/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const selectors = require("devtools/client/performance-new/store/selectors");
const { recordingState: {
  AVAILABLE_TO_RECORD,
  REQUEST_TO_START_RECORDING,
  REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER,
  REQUEST_TO_STOP_PROFILER,
}} = require("devtools/client/performance-new/utils");

/**
 * The recording state manages the current state of the recording panel.
 * @param {string} state - A valid state in `recordingState`.
 * @param {object} options
 */
const changeRecordingState = exports.changeRecordingState =
  (state, options = { didRecordingUnexpectedlyStopped: false }) => ({
    type: "CHANGE_RECORDING_STATE",
    state,
    didRecordingUnexpectedlyStopped: options.didRecordingUnexpectedlyStopped
  });

/**
 * This is the result of the initial questions about the state of the profiler.
 *
 * @param {boolean} isSupportedPlatform - This is a supported platform.
 * @param {string} recordingState - A valid state in `recordingState`.
 */
exports.reportProfilerReady = (isSupportedPlatform, recordingState) => ({
  type: "REPORT_PROFILER_READY",
  isSupportedPlatform,
  recordingState,
});

/**
 * Updates the recording settings for the interval.
 * @param {number} interval
 */
exports.changeInterval = interval => ({
  type: "CHANGE_INTERVAL",
  interval
});

/**
 * Updates the recording settings for the entries.
 * @param {number} entries
 */
exports.changeEntries = entries => ({
  type: "CHANGE_ENTRIES",
  entries
});

/**
 * Updates the recording settings for the features.
 * @param {object} features
 */
exports.changeFeatures = features => ({
  type: "CHANGE_FEATURES",
  features
});

/**
 * Updates the recording settings for the threads.
 * @param {array} threads
 */
exports.changeThreads = threads => ({
  type: "CHANGE_THREADS",
  threads
});

/**
 * Receive the values to intialize the store. See the reducer for what values
 * are expected.
 * @param {object} threads
 */
exports.initializeStore = values => ({
  type: "INITIALIZE_STORE",
  values
});

/**
 * Start a new recording with the perfFront and update the internal recording state.
 */
exports.startRecording = () => {
  return (dispatch, getState) => {
    const recordingSettings = selectors.getRecordingSettings(getState());
    const perfFront = selectors.getPerfFront(getState());
    perfFront.startProfiler(recordingSettings);
    dispatch(changeRecordingState(REQUEST_TO_START_RECORDING));
  };
};

/**
 * Stops the profiler, and opens the profile in a new window.
 */
exports.getProfileAndStopProfiler = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState(REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER));
    const profile = await perfFront.getProfileAndStopProfiler();
    selectors.getReceiveProfileFn(getState())(profile);
    dispatch(changeRecordingState(AVAILABLE_TO_RECORD));
  };
};

/**
 * Stops the profiler, but does not try to retrieve the profile.
 */
exports.stopProfilerAndDiscardProfile = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState(REQUEST_TO_STOP_PROFILER));
    perfFront.stopProfilerAndDiscardProfile();
  };
};
