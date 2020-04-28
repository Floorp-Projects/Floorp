/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 * @typedef {import("../@types/perf").InitializedValues} InitializedValues
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").ReceiveProfile} ReceiveProfile
 * @typedef {import("../@types/perf").SetRecordingPreferences} SetRecordingPreferences
 * @typedef {import("../@types/perf").GetSymbolTableCallback} GetSymbolTableCallback
 * @typedef {import("../@types/perf").RestartBrowserWithEnvironmentVariable} RestartBrowserWithEnvironmentVariable
 * @typedef {import("../@types/perf").GetEnvironmentVariable} GetEnvironmentVariable
 * @typedef {import("../@types/perf").PageContext} PageContext
 * @typedef {import("../@types/perf").Presets} Presets
 */
/**
 * @template S
 * @typedef {import("../@types/perf").Selector<S>} Selector<S>
 */

/** @type {Selector<RecordingState>} */
const getRecordingState = state => state.recordingState;

/** @type {Selector<boolean>} */
const getRecordingUnexpectedlyStopped = state =>
  state.recordingUnexpectedlyStopped;

/** @type {Selector<boolean>} */
const getIsSupportedPlatform = state => state.isSupportedPlatform;

/** @type {Selector<number>} */
const getInterval = state => state.interval;

/** @type {Selector<number>} */
const getEntries = state => state.entries;

/** @type {Selector<string[]>} */
const getFeatures = state => state.features;

/** @type {Selector<string[]>} */
const getThreads = state => state.threads;

/** @type {Selector<string>} */
const getThreadsString = state => getThreads(state).join(",");

/** @type {Selector<string[]>} */
const getObjdirs = state => state.objdirs;

/** @type {Selector<Presets>} */
const getPresets = state => getInitializedValues(state).presets;

/** @type {Selector<string>} */
const getPresetName = state => state.presetName;

/**
 * When remote profiling, there will be a back button to the settings.
 *
 * @type {Selector<(() => void) | undefined>}
 */
const getOpenRemoteDevTools = state =>
  getInitializedValues(state).openRemoteDevTools;

/**
 * Get the functon to open about:profiling. This assumes that the function exists,
 * otherwise it will throw an error.
 *
 * @type {Selector<() => void>}
 */
const getOpenAboutProfiling = state => {
  const { openAboutProfiling } = getInitializedValues(state);
  if (!openAboutProfiling) {
    throw new Error("Expected to get an openAboutProfiling function.");
  }
  return openAboutProfiling;
};

/**
 * Warning! This function returns a new object on every run, and so should not
 * be used directly as a React prop.
 *
 * @type {Selector<RecordingStateFromPreferences>}
 */
const getRecordingSettings = state => {
  const presets = getPresets(state);
  const presetName = getPresetName(state);
  const preset = presets[presetName];
  if (preset) {
    // Use the the settings from the preset.
    return {
      presetName: presetName,
      entries: preset.entries,
      interval: preset.interval,
      features: preset.features,
      threads: preset.threads,
      objdirs: getObjdirs(state),
      // The client doesn't implement durations yet. See Bug 1587165.
      duration: preset.duration,
    };
  }

  // Use the the custom settings from the panel.
  return {
    presetName: "custom",
    entries: getEntries(state),
    interval: getInterval(state),
    features: getFeatures(state),
    threads: getThreads(state),
    objdirs: getObjdirs(state),
    // The client doesn't implement durations yet. See Bug 1587165.
    duration: 0,
  };
};

/** @type {Selector<InitializedValues>} */
const getInitializedValues = state => {
  const values = state.initializedValues;
  if (!values) {
    throw new Error("The store must be initialized before it can be used.");
  }
  return values;
};

/** @type {Selector<PerfFront>} */
const getPerfFront = state => getInitializedValues(state).perfFront;

/** @type {Selector<ReceiveProfile>} */
const getReceiveProfileFn = state => getInitializedValues(state).receiveProfile;

/** @type {Selector<SetRecordingPreferences>} */
const getSetRecordingPreferencesFn = state =>
  getInitializedValues(state).setRecordingPreferences;

/** @type {Selector<PageContext>} */
const getPageContext = state => getInitializedValues(state).pageContext;

/** @type {Selector<(profile: MinimallyTypedGeckoProfile) => GetSymbolTableCallback>} */
const getSymbolTableGetter = state =>
  getInitializedValues(state).getSymbolTableGetter;

/** @type {Selector<string[] | null>} */
const getSupportedFeatures = state =>
  getInitializedValues(state).supportedFeatures;

/** @type {Selector<string | null>} */
const getPromptEnvRestart = state => state.promptEnvRestart;

module.exports = {
  getRecordingState,
  getRecordingUnexpectedlyStopped,
  getIsSupportedPlatform,
  getInterval,
  getEntries,
  getFeatures,
  getThreads,
  getThreadsString,
  getObjdirs,
  getPresets,
  getPresetName,
  getOpenRemoteDevTools,
  getOpenAboutProfiling,
  getRecordingSettings,
  getInitializedValues,
  getPerfFront,
  getReceiveProfileFn,
  getSetRecordingPreferencesFn,
  getPageContext,
  getSymbolTableGetter,
  getPromptEnvRestart,
  getSupportedFeatures,
};
