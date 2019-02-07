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
const { OS } = require("resource://gre/modules/osfile.jsm");
const { ProfilerGetSymbols } = require("resource://app/modules/ProfilerGetSymbols.jsm");

/**
 * The recording state manages the current state of the recording panel.
 * @param {string} state - A valid state in `recordingState`.
 * @param {object} options
 */
const changeRecordingState = exports.changeRecordingState =
  (state, options = { didRecordingUnexpectedlyStopped: false }) => ({
    type: "CHANGE_RECORDING_STATE",
    state,
    didRecordingUnexpectedlyStopped: options.didRecordingUnexpectedlyStopped,
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
 * Dispatch the given action, and then update the recording settings.
 * @param {object} action
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
    const setRecordingPreferences = selectors.getSetRecordingPreferencesFn(getState());
    const recordingSettings = selectors.getRecordingSettings(getState());
    setRecordingPreferences(recordingSettings);
  };
}

/**
 * Updates the recording settings for the interval.
 * @param {number} interval
 */
exports.changeInterval = interval => _dispatchAndUpdatePreferences({
  type: "CHANGE_INTERVAL",
  interval,
});

/**
 * Updates the recording settings for the entries.
 * @param {number} entries
 */
exports.changeEntries = entries => _dispatchAndUpdatePreferences({
  type: "CHANGE_ENTRIES",
  entries,
});

/**
 * Updates the recording settings for the features.
 * @param {object} features
 */
exports.changeFeatures = features => _dispatchAndUpdatePreferences({
  type: "CHANGE_FEATURES",
  features,
});

/**
 * Updates the recording settings for the threads.
 * @param {array} threads
 */
exports.changeThreads = threads => _dispatchAndUpdatePreferences({
  type: "CHANGE_THREADS",
  threads,
});

/**
 * Updates the recording settings for the objdirs.
 * @param {array} objdirs
 */
exports.changeObjdirs = objdirs => _dispatchAndUpdatePreferences({
  type: "CHANGE_OBJDIRS",
  objdirs,
});

/**
 * Receive the values to intialize the store. See the reducer for what values
 * are expected.
 * @param {object} threads
 */
exports.initializeStore = values => ({
  type: "INITIALIZE_STORE",
  ...values,
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
 * Returns a function getDebugPathFor(debugName, breakpadId) => Library which
 * resolves a (debugName, breakpadId) pair to the library's information, which
 * contains the absolute paths on the file system where the binary and its
 * optional pdb file are stored.
 *
 * This is needed for the following reason:
 *  - In order to obtain a symbol table for a system library, we need to know
 *    the library's absolute path on the file system. On Windows, we
 *    additionally need to know the absolute path to the library's PDB file,
 *    which we call the binary's "debugPath".
 *  - Symbol tables are requested asynchronously, by the profiler UI, after the
 *    profile itself has been obtained.
 *  - When the symbol tables are requested, we don't want the profiler UI to
 *    pass us arbitrary absolute file paths, as an extra defense against
 *    potential information leaks.
 *  - Instead, when the UI requests symbol tables, it identifies the library
 *    with a (debugName, breakpadId) pair. We need to map that pair back to the
 *    absolute paths.
 *  - We get the "trusted" paths from the "libs" sections of the profile. We
 *    trust these paths because we just obtained the profile directly from
 *    Gecko.
 *  - This function builds the (debugName, breakpadId) => Library mapping and
 *    retains it on the returned closure so that it can be consulted after the
 *    profile has been passed to the UI.
 *
 * @param {object} profile - The profile JSON object
 */
function createLibraryMap(profile) {
  const map = new Map();
  function fillMapForProcessRecursive(processProfile) {
    for (const lib of processProfile.libs) {
      const { debugName, breakpadId } = lib;
      const key = [debugName, breakpadId].join(":");
      map.set(key, lib);
    }
    for (const subprocess of processProfile.processes) {
      fillMapForProcessRecursive(subprocess);
    }
  }

  fillMapForProcessRecursive(profile);
  return function getLibraryFor(debugName, breakpadId) {
    const key = [debugName, breakpadId].join(":");
    return map.get(key);
  };
}

async function getSymbolTableFromDebuggee(perfFront, path, breakpadId) {
  const [addresses, index, buffer] =
    await perfFront.getSymbolTable(path, breakpadId);
  // The protocol transmits these arrays as plain JavaScript arrays of
  // numbers, but we want to pass them on as typed arrays. Convert them now.
  return [
    new Uint32Array(addresses),
    new Uint32Array(index),
    new Uint8Array(buffer),
  ];
}

async function doesFileExistAtPath(path) {
  try {
    const result = await OS.File.stat(path);
    return !result.isDir;
  } catch (e) {
    if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
      return false;
    }
    throw e;
  }
}

/**
 * Stops the profiler, and opens the profile in a new window.
 */
exports.getProfileAndStopProfiler = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState(REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER));
    const profile = await perfFront.getProfileAndStopProfiler();

    const libraryGetter = createLibraryMap(profile);
    async function getSymbolTable(debugName, breakpadId) {
      const {path, debugPath} = libraryGetter(debugName, breakpadId);
      if (await doesFileExistAtPath(path)) {
        // This profile was obtained from this machine, and not from a
        // different device (e.g. an Android phone). Dump symbols from the file
        // on this machine directly.
        return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
      }
      // The file does not exist, which probably indicates that the profile was
      // obtained on a different machine, i.e. the debuggee is truly remote.
      // Try to obtain the symbol table on the debuggee.
      // We also get into this branch if we're looking up symbol tables for
      // Android system libraries, for example.
      // For now, this path is not used on Windows, which is why we don't need
      // to pass the library's debugPath.
      return getSymbolTableFromDebuggee(perfFront, path, breakpadId);
    }

    selectors.getReceiveProfileFn(getState())(profile, getSymbolTable);
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
