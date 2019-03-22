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
const { ProfilerGetSymbols } = require("resource://gre/modules/ProfilerGetSymbols.jsm");

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
 * Retrieve a symbol table from a binary on the host machine, by looking up
 * relevant build artifacts in the specified objdirs.
 * This is needed if the debuggee is a build running on a remote machine that
 * was compiled by the developer on *this* machine (the "host machine"). In
 * that case, the objdir will contain the compiled binary with full symbol and
 * debug information, whereas the binary on the device may not exist in
 * uncompressed form or may have been stripped of debug information and some
 * symbol information.
 * An objdir, or "object directory", is a directory on the host machine that's
 * used to store build artifacts ("object files") from the compilation process.
 *
 * @param {array of string} objdirs An array of objdir paths on the host machine
 *   that should be searched for relevant build artifacts.
 * @param {string} filename The file name of the binary.
 * @param {string} breakpadId The breakpad ID of the binary.
 * @returns {Promise} The symbol table of the first encountered binary with a
 *   matching breakpad ID, in SymbolTableAsTuple format. An exception is thrown (the
 *   promise is rejected) if nothing was found.
 */
async function getSymbolTableFromLocalBinary(objdirs, filename, breakpadId) {
  const candidatePaths = [];
  for (const objdirPath of objdirs) {
    // Binaries are usually expected to exist at objdir/dist/bin/filename.
    candidatePaths.push(OS.Path.join(objdirPath, "dist", "bin", filename));
    // Also search in the "objdir" directory itself (not just in dist/bin).
    // If, for some unforeseen reason, the relevant binary is not inside the
    // objdirs dist/bin/ directory, this provides a way out because it lets the
    // user specify the actual location.
    candidatePaths.push(OS.Path.join(objdirPath, filename));
  }

  for (const path of candidatePaths) {
    if (await doesFileExistAtPath(path)) {
      try {
        return await ProfilerGetSymbols.getSymbolTable(path, path, breakpadId);
      } catch (e) {
        // ProfilerGetSymbols.getSymbolTable was unsuccessful. So either the
        // file wasn't parseable or its contents didn't match the specified
        // breakpadId, or some other error occurred.
        // Advance to the next candidate path.
      }
    }
  }
  throw new Error("Could not find any matching binary.");
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
      const {name, path, debugPath} = libraryGetter(debugName, breakpadId);
      if (await doesFileExistAtPath(path)) {
        // This profile was obtained from this machine, and not from a
        // different device (e.g. an Android phone). Dump symbols from the file
        // on this machine directly.
        return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
      }
      // The file does not exist, which probably indicates that the profile was
      // obtained on a different machine, i.e. the debuggee is truly remote
      // (e.g. on an Android phone).
      try {
        // First, try to find a binary with a matching file name and breakpadId
        // on the host machine. This works if the profiled build is a developer
        // build that has been compiled on this machine, and if the binary is
        // one of the Gecko binaries and not a system library.
        // The other place where we could obtain symbols is the debuggee device;
        // that's handled in the catch branch below.
        // We check the host machine first, because if this is a developer
        // build, then the objdir files will contain more symbol information
        // than the files that get pushed to the device.
        const objdirs = selectors.getObjdirs(getState());
        return await getSymbolTableFromLocalBinary(objdirs, name, breakpadId);
      } catch (e) {
        // No matching file was found on the host machine.
        // Try to obtain the symbol table on the debuggee. We get into this
        // branch in the following cases:
        //  - Android system libraries
        //  - Firefox binaries that have no matching equivalent on the host
        //    machine, for example because the user didn't point us at the
        //    corresponding objdir, or if the build was compiled somewhere
        //    else, or if the build on the device is outdated.
        // For now, this path is not used on Windows, which is why we don't
        // need to pass the library's debugPath.
        return getSymbolTableFromDebuggee(perfFront, path, breakpadId);
      }
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
