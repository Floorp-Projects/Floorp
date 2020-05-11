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

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);
// For some reason TypeScript was giving me an error when de-structuring AppConstants. I
// suspect a bug in TypeScript was at play.
const AppConstants = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
).AppConstants;

/**
 * @typedef {import("../@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 * @typedef {import("../@types/perf").PopupBackgroundFeatures} PopupBackgroundFeatures
 * @typedef {import("../@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("../@types/perf").Library} Library
 * @typedef {import("../@types/perf").PerformancePref} PerformancePref
 * @typedef {import("../@types/perf").ProfilerWebChannel} ProfilerWebChannel
 * @typedef {import("../@types/perf").MessageFromFrontend} MessageFromFrontend
 * @typedef {import("../@types/perf").PageContext} PageContext
 * @typedef {import("../@types/perf").Presets} Presets
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
/** @type {PerformancePref["Preset"]} */
const PRESET_PREF = "devtools.performance.recording.preset";
/** @type {PerformancePref["PopupFeatureFlag"]} */
const POPUP_FEATURE_FLAG_PREF = "devtools.performance.popup.feature-flag";

// Lazily load the require function, when it's needed.
ChromeUtils.defineModuleGetter(
  this,
  "require",
  "resource://devtools/shared/Loader.jsm"
);

// The following utilities are lazily loaded as they are not needed when controlling the
// global state of the profiler, and only are used during specific funcationality like
// symbolication or capturing a profile.
const lazy = createLazyLoaders({
  OS: () => ChromeUtils.import("resource://gre/modules/osfile.jsm"),
  Utils: () => require("devtools/client/performance-new/utils"),
  BrowserModule: () => require("devtools/client/performance-new/browser"),
  RecordingUtils: () =>
    require("devtools/shared/performance-new/recording-utils"),
  CustomizableUI: () =>
    ChromeUtils.import("resource:///modules/CustomizableUI.jsm"),
  PerfSymbolication: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/symbolication.jsm.js"
    ),
  PreferenceManagement: () =>
    require("devtools/client/performance-new/preference-management"),
  ProfilerMenuButton: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
    ),
});

/** @type {Presets} */
const presets = {
  "web-developer": {
    label: "Web Developer",
    description:
      "Recommended preset for most web app debugging, with low overhead.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
  },
  "firefox-platform": {
    label: "Firefox Platform",
    description: "Recommended preset for internal Firefox platform debugging.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js", "leaf", "stackwalk", "java"],
    threads: ["GeckoMain", "Compositor", "Renderer"],
    duration: 0,
  },
  "firefox-front-end": {
    label: "Firefox Front-End",
    description: "Recommended preset for internal Firefox front-end debugging.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js", "leaf", "stackwalk", "java"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
  },
  media: {
    label: "Media",
    description: "Recommended preset for diagnosing audio and video problems.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["js", "leaf", "stackwalk"],
    threads: [
      "GeckoMain",
      "Compositor",
      "Renderer",
      "RenderBackend",
      "AudioIPC",
      "MediaPDecoder",
      "MediaTimer",
      "MediaPlayback",
      "MediaDecoderStateMachine",
      "AsyncCubebTask",
    ],
    duration: 0,
  },
};

/**
 * This Map caches the symbols from the shared libraries.
 * @type {Map<string, Library>}
 */
const symbolCache = new Map();

/**
 * @param {PageContext} pageContext
 * @param {string} debugName
 * @param {string} breakpadId
 */
async function getSymbolsFromThisBrowser(pageContext, debugName, breakpadId) {
  if (symbolCache.size === 0) {
    // Prime the symbols cache.
    for (const lib of Services.profiler.sharedLibraries) {
      symbolCache.set(`${lib.debugName}/${lib.breakpadId}`, lib);
    }
  }

  const cachedLib = symbolCache.get(`${debugName}/${breakpadId}`);
  if (!cachedLib) {
    throw new Error(
      `The library ${debugName} ${breakpadId} is not in the ` +
        "Services.profiler.sharedLibraries list, so the local path for it is not known " +
        "and symbols for it can not be obtained. This usually happens if a content " +
        "process uses a library that's not used in the parent process - " +
        "Services.profiler.sharedLibraries only knows about libraries in the " +
        "parent process."
    );
  }

  const lib = cachedLib;
  const objdirs = getObjdirPrefValue(pageContext);
  const { getSymbolTableMultiModal } = lazy.PerfSymbolication();
  return getSymbolTableMultiModal(lib, objdirs);
}

/**
 * This function is called directly by devtools/startup/DevToolsStartup.jsm when
 * using the shortcut keys to capture a profile.
 * @param {PageContext} pageContext
 * @return {Promise<void>}
 */
async function captureProfile(pageContext) {
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

  const receiveProfile = lazy.BrowserModule().receiveProfile;
  receiveProfile(profile, (debugName, breakpadId) => {
    return getSymbolsFromThisBrowser(pageContext, debugName, breakpadId);
  });

  Services.profiler.StopProfiler();
}

/**
 * This function is only called by devtools/startup/DevToolsStartup.jsm when
 * starting the profiler using the shortcut keys, through toggleProfiler below.
 * @param {PageContext} pageContext
 */
function startProfiler(pageContext) {
  const { translatePreferencesToState } = lazy.PreferenceManagement();
  const {
    entries,
    interval,
    features,
    threads,
    duration,
  } = translatePreferencesToState(
    getRecordingPreferences(pageContext, Services.profiler.GetFeatures())
  );

  // Get the active BrowsingContext ID from browser.
  const { getActiveBrowsingContextID } = lazy.RecordingUtils();
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
 * @param {PageContext} pageContext
 * @return {void}
 */
function toggleProfiler(pageContext) {
  if (Services.profiler.IsActive()) {
    stopProfiler();
  } else {
    startProfiler(pageContext);
  }
}

/**
 * @param {PageContext} pageContext
 */
function restartProfiler(pageContext) {
  stopProfiler();
  startProfiler(pageContext);
}

/**
 * @param {string} prefName
 * @return {string[]}
 */
function _getArrayOfStringsPref(prefName) {
  const text = Services.prefs.getCharPref(prefName);
  return JSON.parse(text);
}

/**
 * @param {string} prefName
 * @return {string[]}
 */
function _getArrayOfStringsHostPref(prefName) {
  const text = Services.prefs.getStringPref(prefName);
  return JSON.parse(text);
}

/**
 * The profiler recording workflow uses two different pref paths. One set of prefs
 * is stored for local profiling, and another for remote profiling. This function
 * decides which to use. The remote prefs have ".remote" appended to the end of
 * their pref names.
 *
 * @param {PageContext} pageContext
 * @returns {string}
 */
function getPrefPostfix(pageContext) {
  switch (pageContext) {
    case "devtools":
    case "aboutprofiling":
      // Don't use any postfix on the prefs.
      return "";
    case "devtools-remote":
    case "aboutprofiling-remote":
      return ".remote";
    default: {
      const { UnhandledCaseError } = lazy.Utils();
      throw new UnhandledCaseError(pageContext, "Page Context");
    }
  }
}

/**
 * @param {PageContext} pageContext
 * @returns {string[]}
 */
function getObjdirPrefValue(pageContext) {
  const postfix = getPrefPostfix(pageContext);
  return _getArrayOfStringsHostPref(OBJDIRS_PREF + postfix);
}

/**
 * @param {PageContext} pageContext
 * @param {string[]} supportedFeatures
 * @returns {RecordingStateFromPreferences}
 */
function getRecordingPreferences(pageContext, supportedFeatures) {
  const postfix = getPrefPostfix(pageContext);

  // If you add a new preference here, please do not forget to update
  // `revertRecordingPreferences` as well.
  const objdirs = getObjdirPrefValue(pageContext);
  const presetName = Services.prefs.getCharPref(PRESET_PREF + postfix);

  // First try to get the values from a preset.
  const recordingPrefs = getRecordingPrefsFromPreset(
    presetName,
    supportedFeatures,
    objdirs
  );
  if (recordingPrefs) {
    return recordingPrefs;
  }

  // Next use the preferences to get the values.
  const entries = Services.prefs.getIntPref(ENTRIES_PREF + postfix);
  const interval = Services.prefs.getIntPref(INTERVAL_PREF + postfix);
  const features = _getArrayOfStringsPref(FEATURES_PREF + postfix);
  const threads = _getArrayOfStringsPref(THREADS_PREF + postfix);
  const duration = Services.prefs.getIntPref(DURATION_PREF + postfix);

  return {
    presetName: "custom",
    entries,
    interval,
    // Validate the features before passing them to the profiler.
    features: features.filter(feature => supportedFeatures.includes(feature)),
    threads,
    objdirs,
    duration,
  };
}

/**
 * @param {string} presetName
 * @param {string[]} supportedFeatures
 * @param {string[]} objdirs
 * @return {RecordingStateFromPreferences | null}
 */
function getRecordingPrefsFromPreset(presetName, supportedFeatures, objdirs) {
  if (presetName === "custom") {
    return null;
  }

  const preset = presets[presetName];
  if (!preset) {
    console.error(`Unknown profiler preset was encountered: "${presetName}"`);
    return null;
  }

  return {
    presetName,
    entries: preset.entries,
    // The interval is stored in preferences as microseconds, but the preset
    // defines it in terms of milliseconds. Make the conversion here.
    interval: preset.interval * 1000,
    // Validate the features before passing them to the profiler.
    features: preset.features.filter(feature =>
      supportedFeatures.includes(feature)
    ),
    threads: preset.threads,
    objdirs,
    duration: preset.duration,
  };
}

/**
 * @param {PageContext} pageContext
 * @param {RecordingStateFromPreferences} prefs
 */
function setRecordingPreferences(pageContext, prefs) {
  const postfix = getPrefPostfix(pageContext);
  Services.prefs.setCharPref(PRESET_PREF + postfix, prefs.presetName);
  Services.prefs.setIntPref(ENTRIES_PREF + postfix, prefs.entries);
  // The interval pref stores the value in microseconds for extra precision.
  Services.prefs.setIntPref(INTERVAL_PREF + postfix, prefs.interval);
  Services.prefs.setCharPref(
    FEATURES_PREF + postfix,
    JSON.stringify(prefs.features)
  );
  Services.prefs.setCharPref(
    THREADS_PREF + postfix,
    JSON.stringify(prefs.threads)
  );
  Services.prefs.setCharPref(
    OBJDIRS_PREF + postfix,
    JSON.stringify(prefs.objdirs)
  );
}

const platform = AppConstants.platform;

/**
 * Revert the recording prefs for both local and remote profiling.
 * @return {void}
 */
function revertRecordingPreferences() {
  for (const postfix of ["", ".remote"]) {
    Services.prefs.clearUserPref(PRESET_PREF + postfix);
    Services.prefs.clearUserPref(ENTRIES_PREF + postfix);
    Services.prefs.clearUserPref(INTERVAL_PREF + postfix);
    Services.prefs.clearUserPref(FEATURES_PREF + postfix);
    Services.prefs.clearUserPref(THREADS_PREF + postfix);
    Services.prefs.clearUserPref(OBJDIRS_PREF + postfix);
    Services.prefs.clearUserPref(DURATION_PREF + postfix);
  }
  Services.prefs.clearUserPref(POPUP_FEATURE_FLAG_PREF);
}

/**
 * Change the prefs based on a preset. This mechanism is used by the popup to
 * easily switch between different settings.
 * @param {string} presetName
 * @param {PageContext} pageContext
 * @param {string[]} supportedFeatures
 * @return {void}
 */
function changePreset(pageContext, presetName, supportedFeatures) {
  const postfix = getPrefPostfix(pageContext);
  const objdirs = _getArrayOfStringsHostPref(OBJDIRS_PREF + postfix);
  let recordingPrefs = getRecordingPrefsFromPreset(
    presetName,
    supportedFeatures,
    objdirs
  );

  if (!recordingPrefs) {
    // No recordingPrefs were found for that preset. Most likely this means this
    // is a custom preset, or it's one that we dont recognize for some reason.
    // Get the preferences from the individual preference values.
    Services.prefs.setCharPref(PRESET_PREF + postfix, presetName);
    recordingPrefs = getRecordingPreferences(pageContext, supportedFeatures);
  }

  setRecordingPreferences(pageContext, recordingPrefs);
}

/**
 * This handler handles any messages coming from the WebChannel from profiler.firefox.com.
 *
 * @param {ProfilerWebChannel} channel
 * @param {string} id
 * @param {any} message
 * @param {MockedExports.WebChannelTarget} target
 */
function handleWebChannelMessage(channel, id, message, target) {
  if (typeof message !== "object" || typeof message.type !== "string") {
    console.error(
      "An malformed message was received by the profiler's WebChannel handler.",
      message
    );
    return;
  }
  const messageFromFrontend = /** @type {MessageFromFrontend} */ (message);
  const { requestId } = messageFromFrontend;
  switch (messageFromFrontend.type) {
    case "STATUS_QUERY": {
      // The content page wants to know if this channel exists. It does, so respond
      // back to the ping.
      const { ProfilerMenuButton } = lazy.ProfilerMenuButton();
      channel.send(
        {
          type: "STATUS_RESPONSE",
          menuButtonIsEnabled: ProfilerMenuButton.isInNavbar(),
          requestId,
        },
        target
      );
      break;
    }
    case "ENABLE_MENU_BUTTON": {
      const { ownerDocument } = target.browser;
      if (!ownerDocument) {
        throw new Error(
          "Could not find the owner document for the current browser while enabling " +
            "the profiler menu button"
        );
      }
      // Ensure the widget is enabled.
      Services.prefs.setBoolPref(POPUP_FEATURE_FLAG_PREF, true);

      // Enable the profiler menu button.
      const { ProfilerMenuButton } = lazy.ProfilerMenuButton();
      ProfilerMenuButton.addToNavbar(ownerDocument);

      // Dispatch the change event manually, so that the shortcuts will also be
      // added.
      const { CustomizableUI } = lazy.CustomizableUI();
      CustomizableUI.dispatchToolboxEvent("customizationchange");

      // Open the popup with a message.
      ProfilerMenuButton.openPopup(ownerDocument);

      // Respond back that we've done it.
      channel.send(
        {
          type: "ENABLE_MENU_BUTTON_DONE",
          requestId,
        },
        target
      );
      break;
    }
    default:
      console.error(
        "An unknown message type was received by the profiler's WebChannel handler.",
        message
      );
  }
}

// Provide a fake module.exports for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).module = { exports: {} };

module.exports = {
  presets,
  captureProfile,
  startProfiler,
  stopProfiler,
  restartProfiler,
  toggleProfiler,
  platform,
  getSymbolsFromThisBrowser,
  getRecordingPreferences,
  setRecordingPreferences,
  revertRecordingPreferences,
  changePreset,
  handleWebChannelMessage,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
