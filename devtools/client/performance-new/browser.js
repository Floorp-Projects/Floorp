/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const Services = require("Services");

const TRANSFER_EVENT = "devtools:perf-html-transfer-profile";
const SYMBOL_TABLE_REQUEST_EVENT = "devtools:perf-html-request-symbol-table";
const SYMBOL_TABLE_RESPONSE_EVENT = "devtools:perf-html-reply-symbol-table";
const UI_BASE_URL_PREF = "devtools.performance.recording.ui-base-url";
const UI_BASE_URL_DEFAULT = "https://profiler.firefox.com";
const OBJDIRS_PREF = "devtools.performance.recording.objdirs";

/**
 * This file contains all of the privileged browser-specific functionality. This helps
 * keep a clear separation between the privileged and non-privileged client code. It
 * is also helpful in being able to mock out browser behavior for tests, without
 * worrying about polluting the browser environment.
 */

/**
 * Once a profile is received from the actor, it needs to be opened up in
 * profiler.firefox.com to be analyzed. This function opens up profiler.firefox.com
 * into a new browser tab, and injects the profile via a frame script.
 *
 * @param {object} profile - The Gecko profile.
 * @param {function} getSymbolTableCallback - A callback function with the signature
 *   (debugName, breakpadId) => Promise<SymbolTableAsTuple>, which will be invoked
 *   when profiler.firefox.com sends SYMBOL_TABLE_REQUEST_EVENT messages to us. This
 *   function should obtain a symbol table for the requested binary and resolve the
 *   returned promise with it.
 */
function receiveProfile(profile, getSymbolTableCallback) {
  // Find the most recently used window, as the DevTools client could be in a variety
  // of hosts.
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  if (!win) {
    throw new Error("No browser window");
  }
  const browser = win.gBrowser;
  Services.focus.activeWindow = win;

  const baseUrl = Services.prefs.getStringPref(
    UI_BASE_URL_PREF,
    UI_BASE_URL_DEFAULT
  );
  const tab = browser.addWebTab(`${baseUrl}/from-addon`, {
    triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({
      userContextId: browser.contentPrincipal.userContextId,
    }),
  });
  browser.selectedTab = tab;
  const mm = tab.linkedBrowser.messageManager;
  mm.loadFrameScript(
    "chrome://devtools/content/performance-new/frame-script.js",
    false
  );
  mm.sendAsyncMessage(TRANSFER_EVENT, profile);
  mm.addMessageListener(SYMBOL_TABLE_REQUEST_EVENT, e => {
    const { debugName, breakpadId } = e.data;
    getSymbolTableCallback(debugName, breakpadId).then(
      result => {
        const [addr, index, buffer] = result;
        mm.sendAsyncMessage(SYMBOL_TABLE_RESPONSE_EVENT, {
          status: "success",
          debugName,
          breakpadId,
          result: [addr, index, buffer],
        });
      },
      error => {
        mm.sendAsyncMessage(SYMBOL_TABLE_RESPONSE_EVENT, {
          status: "error",
          debugName,
          breakpadId,
          error: `${error}`,
        });
      }
    );
  });
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

  if (
    Array.isArray(array) &&
    array.every(feature => typeof feature === "string")
  ) {
    return array;
  }

  return defaultValue;
}

/**
 * Similar to _getArrayOfStringsPref, but gets the pref from the host browser
 * instance, *not* from the debuggee.
 * Don't trust that the user has stored the correct value in preferences, or that it
 * even exists. Gracefully handle malformed data or missing data. Ensure that this
 * function always returns a valid array of strings.
 * @param {string} prefName
 * @param {array of string} defaultValue
 */
async function _getArrayOfStringsHostPref(prefName, defaultValue) {
  let array;
  try {
    const text = Services.prefs.getStringPref(
      prefName,
      JSON.stringify(defaultValue)
    );
    array = JSON.parse(text);
  } catch (error) {
    return defaultValue;
  }

  if (
    Array.isArray(array) &&
    array.every(feature => typeof feature === "string")
  ) {
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
  const [entries, interval, features, threads, objdirs] = await Promise.all([
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
    _getArrayOfStringsHostPref(OBJDIRS_PREF, defaultSettings.objdirs),
  ]);

  // The pref stores the value in usec.
  const newInterval = interval / 1000;
  return { entries, interval: newInterval, features, threads, objdirs };
}

/**
 * Take the recording settings, as defined by the getRecordingSettings selector, and
 * persist them to preferences. Some of these prefs get persisted on the debuggee,
 * and some of them on the host browser instance.
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
      // The pref stores the value in usec.
      settings.interval * 1000
    ),
    preferenceFront.setCharPref(
      `devtools.performance.recording.features`,
      JSON.stringify(settings.features)
    ),
    preferenceFront.setCharPref(
      `devtools.performance.recording.threads`,
      JSON.stringify(settings.threads)
    ),
    Services.prefs.setCharPref(OBJDIRS_PREF, JSON.stringify(settings.objdirs)),
  ]);
}

module.exports = {
  receiveProfile,
  getRecordingPreferences,
  setRecordingPreferences,
};
