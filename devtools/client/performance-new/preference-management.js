/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("./@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 */

/*
 * This file centralizes some functions needed to manage the preferences for the
 * profiler UI inside Firefox.
 * Especially we store the current settings as defined by the user in
 * preferences: interval, features, threads, etc.
 * However the format we use in the preferences isn't always the same as the one
 * we use in settings.
 * Especially the "interval" setting is handled differently. Indeed, because we
 * can't store float data in Firefox preferences, we store it as an integer with
 * the microseconds (µs) unit. But in the UI we handle it with the milliseconds
 * (ms) unit as a float. That's why there are these multiplication and division
 * by 1000 for that property in this file.
 */

/**
 * This function does a translation on each property if needed. Indeed the
 * stored prefs sometimes have a different shape.
 * This function takes the preferences stored in the user profile and returns
 * the preferences to be used in the redux state.
 * @param {RecordingStateFromPreferences} preferences
 * @returns {RecordingStateFromPreferences}
 */
function translatePreferencesToState(preferences) {
  return {
    ...preferences,
    interval: preferences.interval / 1000, // converts from µs to ms
  };
}

/**
 * This function does a translation on each property if needed. Indeed the
 * stored prefs sometimes have a different shape.
 * This function takes the preferences from the redux state and returns the
 * preferences to be stored in the user profile.
 * @param {RecordingStateFromPreferences} state
 * @returns {RecordingStateFromPreferences}
 */
function translatePreferencesFromState(state) {
  return {
    ...state,
    interval: state.interval * 1000, // converts from ms to µs
  };
}

module.exports = {
  translatePreferencesToState,
  translatePreferencesFromState,
};
