/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * This file contains all of the background logic for controlling the state and
 * configuration of the profiler. It is in a JSM so that the logic can be shared
 * with both the popup client, and the keyboard shortcuts. The shortcuts don't need
 * access to any UI, and need to be loaded independent of the popup.
 */

// The following are not lazily loaded as they are needed during initialization.

/** @type {import("resource://gre/modules/Services.jsm")} */
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
/** @type {import("resource://gre/modules/AppConstants.jsm")} */
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

/**
 * @typedef {import("../@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 * @typedef {import("../@types/perf").PopupBackgroundFeatures} PopupBackgroundFeatures
 * @typedef {import("../@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("../@types/perf").PerformancePref} PerformancePref
 */

/** @type {PerformancePref["Entries"]} */
const ENTRIES_PREF = "devtools.performance.recording.entries";
/** @type {PerformancePref["Interval"]} */
const INTERVAL_PREF = "devtools.performance.recording.interval";
/** @type {PerformancePref["Features"]} */
const FEATURES_PREF = "devtools.performance.recording.features";
/** @type {PerformancePref["Threads"]} */
const THREADS_PREF = "devtools.performance.recording.threads";
/** @type {PerformancePref["ObjDirs"]} */
const OBJDIRS_PREF = "devtools.performance.recording.objdirs";
/** @type {PerformancePref["Duration"]} */
const DURATION_PREF = "devtools.performance.recording.duration";

// The following utilities are lazily loaded as they are not needed when controlling the
// global state of the profiler, and only are used during specific funcationality like
// symbolication or capturing a profile.

/**
 * TS-TODO
 *
 * This function replaces lazyRequireGetter, and TypeScript can understand it. It's
 * currently duplicated until we have consensus that TypeScript is a good idea.
 *
 * @template T
 * @type {(callback: () => T) => () => T}
 */
function requireLazy(callback) {
  /** @type {T | undefined} */
  let cache;
  return () => {
    if (cache === undefined) {
      cache = callback();
    }
    return cache;
  };
}

const lazyOS = requireLazy(() =>
  /** @type {import("resource://gre/modules/osfile.jsm")} */
  (ChromeUtils.import("resource://gre/modules/osfile.jsm"))
);

const lazyProfilerGetSymbols = requireLazy(() =>
  /** @type {import("resource://gre/modules/ProfilerGetSymbols.jsm")} */
  (ChromeUtils.import("resource://gre/modules/ProfilerGetSymbols.jsm"))
);

const lazyBrowserModule = requireLazy(() => {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  /** @type {import("devtools/client/performance-new/browser")} */
  const browserModule = require("devtools/client/performance-new/browser");
  return browserModule;
});

const lazyPreferenceManagement = requireLazy(() => {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );

  /** @type {import("devtools/client/performance-new/preference-management")} */
  const preferenceManagementModule = require("devtools/client/performance-new/preference-management");
  return preferenceManagementModule;
});

/**
 * This Map caches the symbols from the shared libraries.
 * @type {Map<string, { path: string, debugPath: string }>}
 */
const symbolCache = new Map();

/**
 * @param {string} debugName
 * @param {string} breakpadId
 */
async function getSymbolsFromThisBrowser(debugName, breakpadId) {
  if (symbolCache.size === 0) {
    // Prime the symbols cache.
    for (const lib of Services.profiler.sharedLibraries) {
      symbolCache.set(`${lib.debugName}/${lib.breakpadId}`, {
        path: lib.path,
        debugPath: lib.debugPath,
      });
    }
  }

  const cachedLibInfo = symbolCache.get(`${debugName}/${breakpadId}`);
  if (!cachedLibInfo) {
    throw new Error(
      `The library ${debugName} ${breakpadId} is not in the ` +
        "Services.profiler.sharedLibraries list, so the local path for it is not known " +
        "and symbols for it can not be obtained. This usually happens if a content " +
        "process uses a library that's not used in the parent process - " +
        "Services.profiler.sharedLibraries only knows about libraries in the " +
        "parent process."
    );
  }

  const { path, debugPath } = cachedLibInfo;
  const { OS } = lazyOS();
  if (!OS.Path.split(path).absolute) {
    throw new Error(
      "Services.profiler.sharedLibraries did not contain an absolute path for " +
        `the library ${debugName} ${breakpadId}, so symbols for this library can not ` +
        "be obtained."
    );
  }

  const { ProfilerGetSymbols } = lazyProfilerGetSymbols();

  return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
}

/**
 * This function is called directly by devtools/startup/DevToolsStartup.jsm when
 * using the shortcut keys to capture a profile.
 * @type {() => Promise<void>}
 */
async function captureProfile() {
  if (!Services.profiler.IsActive()) {
    // The profiler is not active, ignore this shortcut.
    return;
  }
  // Pause profiler before we collect the profile, so that we don't capture
  // more samples while the parent process waits for subprocess profiles.
  Services.profiler.PauseSampling();

  const profile = await Services.profiler
    .getProfileDataAsGzippedArrayBuffer()
    .catch(
      /** @type {(e: any) => {}} */ e => {
        console.error(e);
        return {};
      }
    );

  const receiveProfile = lazyBrowserModule().receiveProfile;
  receiveProfile(profile, getSymbolsFromThisBrowser);

  Services.profiler.StopProfiler();
}

/**
 * This function is only called by devtools/startup/DevToolsStartup.jsm when
 * starting the profiler using the shortcut keys, through toggleProfiler below.
 */
function startProfiler() {
  const { translatePreferencesToState } = lazyPreferenceManagement();
  const {
    entries,
    interval,
    features,
    threads,
    duration,
  } = translatePreferencesToState(getRecordingPreferencesFromBrowser());

  // Get the active BrowsingContext ID from browser.
  const getActiveBrowsingContextID = lazyBrowserModule()
    .getActiveBrowsingContextID;
  const activeBrowsingContextID = getActiveBrowsingContextID();

  Services.profiler.StartProfiler(
    entries,
    interval,
    features,
    threads,
    activeBrowsingContextID,
    duration
  );
}

/**
 * This function is called directly by devtools/startup/DevToolsStartup.jsm when
 * using the shortcut keys to capture a profile.
 * @type {() => void}
 */
function stopProfiler() {
  Services.profiler.StopProfiler();
}

/**
 * This function is called directly by devtools/startup/DevToolsStartup.jsm when
 * using the shortcut keys to start and stop the profiler.
 * @type {() => void}
 */
function toggleProfiler() {
  if (Services.profiler.IsActive()) {
    stopProfiler();
  } else {
    startProfiler();
  }
}

/**
 * @type {() => void}
 */
function restartProfiler() {
  stopProfiler();
  startProfiler();
}

/**
 * @param {string} prefName
 * @param {string[]} defaultValue
 * @return {string[]}
 */
function _getArrayOfStringsPref(prefName, defaultValue) {
  let array;
  try {
    const text = Services.prefs.getCharPref(prefName);
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
 * @param {string} prefName
 * @param {string[]} defaultValue
 * @return {string[]}
 */
function _getArrayOfStringsHostPref(prefName, defaultValue) {
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
 * A simple cache for the default recording preferences.
 * @type {RecordingStateFromPreferences}
 */
let _defaultPrefs;

/**
 * This function contains the canonical defaults for the data store in the
 * preferences in the user profile. They represent the default values for both
 * the popup and panel's recording settings.
 */
function getDefaultRecordingPreferences() {
  if (!_defaultPrefs) {
    _defaultPrefs = {
      entries: 10000000, // ~80mb,
      // Do not expire markers, let them roll off naturally from the circular buffer.
      duration: 0,
      interval: 1000, // 1000µs = 1ms
      features: ["js", "leaf", "responsiveness", "stackwalk"],
      threads: ["GeckoMain", "Compositor"],
      objdirs: [],
    };

    if (AppConstants.platform === "android") {
      // Java profiling is only meaningful on android.
      _defaultPrefs.features.push("java");
    }
  }

  return _defaultPrefs;
}

/**
 * @returns {RecordingStateFromPreferences}
 */
function getRecordingPreferencesFromBrowser() {
  const defaultPrefs = getDefaultRecordingPreferences();

  const entries = Services.prefs.getIntPref(ENTRIES_PREF, defaultPrefs.entries);
  const interval = Services.prefs.getIntPref(
    INTERVAL_PREF,
    defaultPrefs.interval
  );
  const features = _getArrayOfStringsPref(FEATURES_PREF, defaultPrefs.features);
  const threads = _getArrayOfStringsPref(THREADS_PREF, defaultPrefs.threads);
  const objdirs = _getArrayOfStringsHostPref(
    OBJDIRS_PREF,
    defaultPrefs.objdirs
  );
  const duration = Services.prefs.getIntPref(
    DURATION_PREF,
    defaultPrefs.duration
  );

  const supportedFeatures = new Set(Services.profiler.GetFeatures());

  return {
    entries,
    interval,
    // Validate the features before passing them to the profiler.
    features: features.filter(feature => supportedFeatures.has(feature)),
    threads,
    objdirs,
    duration,
  };
}

/**
 * @param {RecordingStateFromPreferences} prefs
 */
function setRecordingPreferencesOnBrowser(prefs) {
  Services.prefs.setIntPref(ENTRIES_PREF, prefs.entries);
  // The interval pref stores the value in microseconds for extra precision.
  Services.prefs.setIntPref(INTERVAL_PREF, prefs.interval);
  Services.prefs.setCharPref(FEATURES_PREF, JSON.stringify(prefs.features));
  Services.prefs.setCharPref(THREADS_PREF, JSON.stringify(prefs.threads));
  Services.prefs.setCharPref(OBJDIRS_PREF, JSON.stringify(prefs.objdirs));
}

const platform = AppConstants.platform;

/**
 * @type {() => void}
 */
function revertRecordingPreferences() {
  setRecordingPreferencesOnBrowser(getDefaultRecordingPreferences());
}

var EXPORTED_SYMBOLS = [
  "captureProfile",
  "startProfiler",
  "stopProfiler",
  "restartProfiler",
  "toggleProfiler",
  "platform",
  "getSymbolsFromThisBrowser",
  "getDefaultRecordingPreferences",
  "getRecordingPreferencesFromBrowser",
  "setRecordingPreferencesOnBrowser",
  "revertRecordingPreferences",
];
