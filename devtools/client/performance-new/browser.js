/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const Services = require("Services");

/**
 * This file contains all of the privileged browser-specific functionality. This helps
 * keep a clear separation between the privileged and non-privileged client code. It
 * is also helpful in being able to mock out browser behavior for tests, without
 * worrying about polluting the browser environment.
 */

/**
 * Once a profile is received from the actor, it needs to be opened up in perf.html
 * to be analyzed. This function opens up perf.html into a new browser tab, and injects
 * the profile via a frame script.
 *
 * @param {object} profile - The Gecko profile.
 */
function receiveProfile(profile) {
  // Find the most recently used window, as the DevTools client could be in a variety
  // of hosts.
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  if (!win) {
    throw new Error("No browser window");
  }
  const browser = win.gBrowser;
  Services.focus.activeWindow = win;

  const tab = browser.addTab("https://perf-html.io/from-addon");
  browser.selectedTab = tab;
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript(
    "chrome://devtools/content/performance-new/frame-script.js",
    false
  );
  mm.sendAsyncMessage("devtools:perf-html-transfer-profile", profile);
}

/**
 * Don't trust that the user has stored the correct value in preferences, or that it
 * even exists. Gracefully handle malformed data or missing data. Ensure that this
 * function always returns a valid array of strings.
 * @param {PreferenceFront} preferenceFront
 * @param {string} prefName
 * @param {array of string} defaultValue
 */
async function _getArrayOfStringsPref(preferenceFront, prefName, defaultValue) {
  let array;
  try {
    const text = await preferenceFront.getCharPref(prefName);
    array = JSON.parse(text);
  } catch (error) {
    return defaultValue;
  }

  if (Array.isArray(array) && array.every(feature => typeof feature === "string")) {
    return array;
  }

  return defaultValue;
}

/**
 * Attempt to get a int preference value from the debuggee.
 *
 * @param {PreferenceFront} preferenceFront
 * @param {string} prefName
 * @param {number} defaultValue
 */
async function _getIntPref(preferenceFront, prefName, defaultValue) {
  try {
    return await preferenceFront.getIntPref(prefName);
  } catch (error) {
    return defaultValue;
  }
}

/**
 * Get the recording settings from the preferences. These settings are stored once
 * for local debug targets, and another set of settings for remote targets. This
 * is helpful for configuring for remote targets like Android phones that may require
 * different features or configurations.
 *
 * @param {PreferenceFront} preferenceFront
 * @param {object} defaultSettings See the getRecordingSettings selector for the shape
 *                                 of the object and how it gets defined.
 */
async function getRecordingPreferences(preferenceFront, defaultSettings = {}) {
  const [ entries, interval, features, threads ] = await Promise.all([
    _getIntPref(
      preferenceFront,
      `devtools.performance.recording.entries`,
      defaultSettings.entries
    ),
    _getIntPref(
      preferenceFront,
      `devtools.performance.recording.interval`,
      defaultSettings.interval
    ),
    _getArrayOfStringsPref(
      preferenceFront,
      `devtools.performance.recording.features`,
      defaultSettings.features
    ),
    _getArrayOfStringsPref(
      preferenceFront,
      `devtools.performance.recording.threads`,
      defaultSettings.threads
    ),
  ]);

  return { entries, interval, features, threads };
}

/**
 * Take the recording settings, as defined by the getRecordingSettings selector, and
 * persist them to preferences.
 *
 * @param {PreferenceFront} preferenceFront
 * @param {object} defaultSettings See the getRecordingSettings selector for the shape
 *                                 of the object and how it gets defined.
 */
async function setRecordingPreferences(preferenceFront, settings) {
  await Promise.all([
    preferenceFront.setIntPref(
      `devtools.performance.recording.entries`,
      settings.entries
    ),
    preferenceFront.setIntPref(
      `devtools.performance.recording.interval`,
      settings.interval
    ),
    preferenceFront.setCharPref(
      `devtools.performance.recording.features`,
      JSON.stringify(settings.features)
    ),
    preferenceFront.setCharPref(
      `devtools.performance.recording.threads`,
      JSON.stringify(settings.threads)
    )
  ]);
}

module.exports = {
  receiveProfile,
  getRecordingPreferences,
  setRecordingPreferences
};
