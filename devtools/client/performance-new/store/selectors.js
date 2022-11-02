/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("../@types/perf").RecordingState} RecordingState
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("../@types/perf").InitializedValues} InitializedValues
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").ReceiveProfile} ReceiveProfile
 * @typedef {import("../@types/perf").RestartBrowserWithEnvironmentVariable} RestartBrowserWithEnvironmentVariable
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

/** @type {Selector<boolean | null>} */
const getIsSupportedPlatform = state => state.isSupportedPlatform;

/** @type {Selector<RecordingSettings>} */
const getRecordingSettings = state => state.recordingSettings;

/** @type {Selector<number>} */
const getInterval = state => getRecordingSettings(state).interval;

/** @type {Selector<number>} */
const getEntries = state => getRecordingSettings(state).entries;

/** @type {Selector<string[]>} */
const getFeatures = state => getRecordingSettings(state).features;

/** @type {Selector<string[]>} */
const getThreads = state => getRecordingSettings(state).threads;

/** @type {Selector<string>} */
const getThreadsString = state => getThreads(state).join(",");

/** @type {Selector<string[]>} */
const getObjdirs = state => getRecordingSettings(state).objdirs;

/** @type {Selector<Presets>} */
const getPresets = state => getInitializedValues(state).presets;

/** @type {Selector<string>} */
const getPresetName = state => state.recordingSettings.presetName;

/**
 * When remote profiling, there will be a back button to the settings.
 *
 * @type {Selector<(() => void) | undefined>}
 */
const getOpenRemoteDevTools = state =>
  getInitializedValues(state).openRemoteDevTools;

/** @type {Selector<InitializedValues>} */
const getInitializedValues = state => {
  const values = state.initializedValues;
  if (!values) {
    throw new Error("The store must be initialized before it can be used.");
  }
  return values;
};

/** @type {Selector<PageContext>} */
const getPageContext = state => getInitializedValues(state).pageContext;

/** @type {Selector<string[]>} */
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
  getRecordingSettings,
  getInitializedValues,
  getPageContext,
  getPromptEnvRestart,
  getSupportedFeatures,
};
