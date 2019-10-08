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

/**
 * Warning! This function returns a new object on every run, and so should not
 * be used directly as a React prop.
 *
 * @type {Selector<RecordingStateFromPreferences>}
 */
const getRecordingSettings = state => {
  return {
    entries: getEntries(state),
    interval: getInterval(state),
    features: getFeatures(state),
    threads: getThreads(state),
    objdirs: getObjdirs(state),
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

/** @type {Selector<boolean>} */
const getIsPopup = state => getInitializedValues(state).isPopup;

/** @type {Selector<(profile: Object) => GetSymbolTableCallback>} */
const getSymbolTableGetter = state =>
  getInitializedValues(state).getSymbolTableGetter;

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
  getRecordingSettings,
  getInitializedValues,
  getPerfFront,
  getReceiveProfileFn,
  getSetRecordingPreferencesFn,
  getIsPopup,
  getSymbolTableGetter,
};
