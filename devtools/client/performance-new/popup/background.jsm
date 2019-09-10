/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file contains all of the background logic for controlling the state and
 * configuration of the profiler. It is in a JSM so that the logic can be shared
 * with both the popup client, and the keyboard shortcuts. The shortcuts don't need
 * access to any UI, and need to be loaded independent of the popup.
 */

// The following are not lazily loaded as they are needed during initialization.f
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

// The following utilities are lazily loaded as they are not needed when controlling the
// global state of the profiler, and only are used during specific funcationality like
// symbolication or capturing a profile.
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "ProfilerGetSymbols",
  "resource://gre/modules/ProfilerGetSymbols.jsm"
);
loader.lazyRequireGetter(
  this,
  "receiveProfile",
  "devtools/client/performance-new/browser",
  true
);

// This pref contains the JSON serialization of the popup's profiler state with
// a string key based off of the debug name and breakpad id.
const PROFILER_STATE_PREF = "devtools.performance.popup";
const DEFAULT_WINDOW_LENGTH = 20; // 20sec
const DEFAULT_INTERVAL = 1; // 1ms
const DEFAULT_BUFFER_SIZE = 10000000; // 90MB
const DEFAULT_THREADS = "GeckoMain,Compositor";
const DEFAULT_STACKWALK_FEATURE = true;

// This Map caches the symbols from the shared libraries.
const symbolCache = new Map();

const primeSymbolStore = libs => {
  for (const { path, debugName, debugPath, breakpadId } of libs) {
    symbolCache.set(`${debugName}/${breakpadId}`, { path, debugPath });
  }
};

const state = initializeState();

const forTestsOnly = {
  DEFAULT_BUFFER_SIZE,
  DEFAULT_STACKWALK_FEATURE,
  initializeState,
  adjustState,
  getState() {
    return state;
  },
  revertPrefs() {
    Services.prefs.clearUserPref(PROFILER_STATE_PREF);
  },
};

function adjustState(newState) {
  // Deep clone the object, since this can be called through popup.xhtml,
  // which can be unloaded thus leaving this object dead.
  newState = JSON.parse(JSON.stringify(newState));
  Object.assign(state, newState);

  try {
    Services.prefs.setStringPref(PROFILER_STATE_PREF, JSON.stringify(state));
  } catch (error) {
    console.error("Unable to save the profiler state for the popup.");
    throw error;
  }
}

async function getSymbols(debugName, breakpadId) {
  if (symbolCache.size === 0) {
    primeSymbolStore(Services.profiler.sharedLibraries);
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
  if (!OS.Path.split(path).absolute) {
    throw new Error(
      "Services.profiler.sharedLibraries did not contain an absolute path for " +
        `the library ${debugName} ${breakpadId}, so symbols for this library can not ` +
        "be obtained."
    );
  }

  return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
}

async function captureProfile() {
  if (!state.isRunning) {
    // The profiler is not active, ignore this shortcut.
    return;
  }
  // Pause profiler before we collect the profile, so that we don't capture
  // more samples while the parent process waits for subprocess profiles.
  Services.profiler.PauseSampling();

  const profile = await Services.profiler
    .getProfileDataAsGzippedArrayBuffer()
    .catch(e => {
      console.error(e);
      return {};
    });

  receiveProfile(profile, getSymbols);

  Services.profiler.StopProfiler();
}

/**
 * Not all features are supported on every version of Firefox. Get the list of checked
 * features, add a few defaults, and filter for what is actually supported.
 */
function getEnabledFeatures(features, threads) {
  const enabledFeatures = Object.keys(features).filter(f => features[f]);
  if (threads.length > 0) {
    enabledFeatures.push("threads");
  }
  const supportedFeatures = Services.profiler.GetFeatures([]);
  return enabledFeatures.filter(feature => supportedFeatures.includes(feature));
}

function startProfiler() {
  const threads = state.threads.split(",");
  const features = getEnabledFeatures(state.features, threads);
  const windowLength =
    state.windowLength !== state.infiniteWindowLength ? state.windowLength : 0;

  const { buffersize, interval } = state;

  Services.profiler.StartProfiler(
    buffersize,
    interval,
    features,
    threads,
    windowLength
  );
}

function stopProfiler() {
  Services.profiler.StopProfiler();
}

function toggleProfiler() {
  if (state.isRunning) {
    stopProfiler();
  } else {
    startProfiler();
  }
}

function restartProfiler() {
  stopProfiler();
  startProfiler();
}

// This running observer was adapted from the web extension.
const isRunningObserver = {
  _observers: new Set(),

  observe(subject, topic, data) {
    switch (topic) {
      case "profiler-started":
      case "profiler-stopped":
        // Make the observer calls asynchronous.
        const isRunningPromise = Promise.resolve(topic === "profiler-started");
        for (const observer of this._observers) {
          isRunningPromise.then(observer);
        }
        break;
    }
  },

  _startListening() {
    Services.obs.addObserver(this, "profiler-started");
    Services.obs.addObserver(this, "profiler-stopped");
  },

  _stopListening() {
    Services.obs.removeObserver(this, "profiler-started");
    Services.obs.removeObserver(this, "profiler-stopped");
  },

  addObserver(observer) {
    if (this._observers.size === 0) {
      this._startListening();
    }

    this._observers.add(observer);
    // Notify the observers the current state asynchronously.
    Promise.resolve(Services.profiler.IsActive()).then(observer);
  },

  removeObserver(observer) {
    if (this._observers.delete(observer) && this._observers.size === 0) {
      this._stopListening();
    }
  },
};

function getStoredStateOrNull() {
  // Pull out the stored state from preferences, it is a raw string.
  const storedStateString = Services.prefs.getStringPref(
    PROFILER_STATE_PREF,
    ""
  );
  if (storedStateString === "") {
    return null;
  }

  try {
    // Attempt to parse the results.
    return JSON.parse(storedStateString);
  } catch (error) {
    console.error(
      `Could not parse the stored state for the profile in the ` +
        `preferences ${PROFILER_STATE_PREF}`
    );
  }
  return null;
}

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

function getRecordingPreferencesFromBrowser(defaultSettings = {}) {
  const [entries, interval, features, threads, objdirs] = [
    Services.prefs.getIntPref(
      `devtools.performance.recording.entries`,
      defaultSettings.entries
    ),
    Services.prefs.getIntPref(
      `devtools.performance.recording.interval`,
      defaultSettings.interval
    ),
    _getArrayOfStringsPref(
      `devtools.performance.recording.features`,
      defaultSettings.features
    ),
    _getArrayOfStringsPref(
      `devtools.performance.recording.threads`,
      defaultSettings.threads
    ),
    _getArrayOfStringsHostPref(
      "devtools.performance.recording.objdirs",
      defaultSettings.objdirs
    ),
  ];

  // The pref stores the value in usec.
  const newInterval = interval / 1000;
  return { entries, interval: newInterval, features, threads, objdirs };
}

function setRecordingPreferencesOnBrowser(settings) {
  Services.prefs.setIntPref(
    `devtools.performance.recording.entries`,
    settings.entries
  );
  Services.prefs.setIntPref(
    `devtools.performance.recording.interval`,
    // The pref stores the value in usec.
    settings.interval * 1000
  );
  Services.prefs.setCharPref(
    `devtools.performance.recording.features`,
    JSON.stringify(settings.features)
  );
  Services.prefs.setCharPref(
    `devtools.performance.recording.threads`,
    JSON.stringify(settings.threads)
  );
  Services.prefs.setCharPref(
    "devtools.performance.recording.objdirs",
    JSON.stringify(settings.objdirs)
  );
}

function initializeState() {
  const features = {
    java: false,
    js: true,
    leaf: true,
    mainthreadio: false,
    privacy: false,
    responsiveness: true,
    screenshots: false,
    seqstyle: false,
    stackwalk: DEFAULT_STACKWALK_FEATURE,
    tasktracer: false,
    trackopts: false,
    jstracer: false,
    preferencereads: false,
    jsallocations: false,
  };

  if (AppConstants.platform === "android") {
    // Java profiling is only meaningful on android.
    features.java = true;
  }

  const storedState = getStoredStateOrNull();

  if (storedState && storedState.features) {
    const storedFeatures = storedState.features;
    // Validate the stored state. It's possible a feature was added or removed
    // since the profiler was last run.
    for (const key of Object.keys(features)) {
      features[key] =
        key in storedFeatures ? Boolean(storedFeatures[key]) : features[key];
    }
  }

  // This function is created inline to make it easy to validate
  // the stored state using the captured storedState value.
  function validateStoredState(key, type, defaultValue) {
    if (!storedState) {
      return defaultValue;
    }
    const storedValue = storedState[key];
    return typeof storedValue === type ? storedValue : defaultValue;
  }

  return {
    // These values are stale, and need to be re-generated.
    isRunning: Services.profiler.IsActive(),
    settingsOpen: false,
    features,

    // Look these up from stored state.
    buffersize: validateStoredState(
      "buffersize",
      "number",
      DEFAULT_BUFFER_SIZE
    ),
    windowLength: validateStoredState(
      "windowLength",
      "number",
      DEFAULT_WINDOW_LENGTH
    ),
    interval: validateStoredState("interval", "number", DEFAULT_INTERVAL),
    threads: validateStoredState("threads", "string", DEFAULT_THREADS),
  };
}

isRunningObserver.addObserver(isRunning => {
  adjustState({ isRunning });
});

const platform = AppConstants.platform;

var EXPORTED_SYMBOLS = [
  "adjustState",
  "captureProfile",
  "state",
  "startProfiler",
  "stopProfiler",
  "restartProfiler",
  "toggleProfiler",
  "isRunningObserver",
  "platform",
  "getRecordingPreferencesFromBrowser",
  "setRecordingPreferencesOnBrowser",
  "forTestsOnly",
];
