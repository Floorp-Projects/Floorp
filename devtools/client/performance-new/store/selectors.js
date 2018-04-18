/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const getRecordingState = state => state.recordingState;
const getRecordingUnexpectedlyStopped = state => state.recordingUnexpectedlyStopped;
const getIsSupportedPlatform = state => state.isSupportedPlatform;
const getInterval = state => state.interval;
const getEntries = state => state.entries;
const getFeatures = state => state.features;
const getThreads = state => state.threads;
const getThreadsString = state => getThreads(state).join(",");

const getRecordingSettings = state => {
  return {
    entries: getEntries(state),
    interval: getInterval(state),
    features: getFeatures(state),
    threads: getThreads(state),
  };
};

const getInitializedValues = state => {
  const values = state.initializedValues;
  if (!values) {
    throw new Error("The store must be initialized before it can be used.");
  }
  return values;
};

const getPerfFront = state => getInitializedValues(state).perfFront;
const getToolbox = state => getInitializedValues(state).toolbox;
const getReceiveProfileFn = state => getInitializedValues(state).receiveProfile;
const getSetRecordingPreferencesFn =
  state => getInitializedValues(state).setRecordingPreferences;

module.exports = {
  getRecordingState,
  getRecordingUnexpectedlyStopped,
  getIsSupportedPlatform,
  getInterval,
  getEntries,
  getFeatures,
  getThreads,
  getThreadsString,
  getRecordingSettings,
  getInitializedValues,
  getPerfFront,
  getToolbox,
  getReceiveProfileFn,
  getSetRecordingPreferencesFn
};
