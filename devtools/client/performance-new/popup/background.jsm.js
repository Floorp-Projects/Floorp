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
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("../@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("../@types/perf").Library} Library
 * @typedef {import("../@types/perf").PerformancePref} PerformancePref
 * @typedef {import("../@types/perf").ProfilerWebChannel} ProfilerWebChannel
 * @typedef {import("../@types/perf").MessageFromFrontend} MessageFromFrontend
 * @typedef {import("../@types/perf").PageContext} PageContext
 * @typedef {import("../@types/perf").PrefPostfix} PrefPostfix
 * @typedef {import("../@types/perf").Presets} Presets
 * @typedef {import("../@types/perf").ProfilerViewMode} ProfilerViewMode
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
  ProfilerMenuButton: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
    ),
});

// TODO - Bug 1681539. The presets still need to be localized.

/** @type {Presets} */
const presets = {
  "web-developer": {
    label: "Web Developer",
    description:
      "Recommended preset for most web app debugging, with low overhead.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js", "cpu"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
    profilerViewMode: "active-tab",
  },
  "firefox-platform": {
    label: "Firefox Platform",
    description: "Recommended preset for internal Firefox platform debugging.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js", "leaf", "stackwalk", "cpu", "java"],
    threads: ["GeckoMain", "Compositor", "Renderer", "SwComposite"],
    duration: 0,
  },
  "firefox-front-end": {
    label: "Firefox Front-End",
    description: "Recommended preset for internal Firefox front-end debugging.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["screenshots", "js", "leaf", "stackwalk", "cpu", "java"],
    threads: ["GeckoMain", "Compositor", "Renderer", "DOM Worker"],
    duration: 0,
  },
  graphics: {
    label: "Firefox Graphics",
    description:
      "Recommended preset for Firefox graphics performance investigation.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["leaf", "stackwalk", "js", "cpu", "java"],
    threads: [
      "GeckoMain",
      "Compositor",
      "Renderer",
      "SwComposite",
      "RenderBackend",
      "SceneBuilder",
      "WrWorker",
      "CanvasWorkers",
    ],
    duration: 0,
  },
  media: {
    label: "Media",
    description: "Recommended preset for diagnosing audio and video problems.",
    entries: 128 * 1024 * 1024,
    interval: 1,
    features: ["js", "leaf", "stackwalk", "cpu", "audiocallbacktracing"],
    threads: [
      "AsyncCubebTask",
      "AudioEncoderQueue",
      "AudioIPC",
      "call_worker_queue",
      "Compositor",
      "DecodingThread",
      "GeckoMain",
      "GraphRunner",
      "IncomingVideoStream",
      "InotifyEventThread",
      "MediaDecoderStateMachine",
      "MediaPDecoder",
      "MediaSupervisor",
      "MediaTimer",
      "ModuleProcessThread",
      "NativeAudioCallback",
      "PacerThread",
      "RenderBackend",
      "Renderer",
      "SwComposite",
      "VoiceProcessThread",
    ],
    duration: 0,
  },
};

/**
 * Return the proper view mode for the Firefox Profiler front-end timeline by
 * looking at the proper preset that is selected.
 * Return value can be undefined when the preset is unknown or custom.
 * @param {PageContext} pageContext
 * @return {ProfilerViewMode | undefined}
 */
function getProfilerViewModeForCurrentPreset(pageContext) {
  const prefPostfix = getPrefPostfix(pageContext);
  const presetName = Services.prefs.getCharPref(PRESET_PREF + prefPostfix);

  if (presetName === "custom") {
    return undefined;
  }

  const preset = presets[presetName];
  if (!preset) {
    console.error(`Unknown profiler preset was encountered: "${presetName}"`);
    return undefined;
  }
  return preset.profilerViewMode;
}

/**
 * This function is called when the profile is captured with the shortcut
 * keys, with the profiler toolbarbutton, or with the button inside the
 * popup.
 * @param {PageContext} pageContext
 * @return {Promise<void>}
 */
async function captureProfile(pageContext) {
  if (!Services.profiler.IsActive()) {
    // The profiler is not active, ignore.
    return;
  }
  if (Services.profiler.IsPaused()) {
    // The profiler is already paused for capture, ignore.
    return;
  }

  // Pause profiler before we collect the profile, so that we don't capture
  // more samples while the parent process waits for subprocess profiles.
  Services.profiler.Pause();

  const profile = await Services.profiler
    .getProfileDataAsGzippedArrayBuffer()
    .catch(
      /** @type {(e: any) => {}} */ e => {
        console.error(e);
        return {};
      }
    );

  const profilerViewMode = getProfilerViewModeForCurrentPreset(pageContext);
  const sharedLibraries = Services.profiler.sharedLibraries;
  const objdirs = getObjdirPrefValue();

  const { createLocalSymbolicationService } = lazy.PerfSymbolication();
  const symbolicationService = createLocalSymbolicationService(
    sharedLibraries,
    objdirs
  );

  const { openProfilerAndDisplayProfile } = lazy.BrowserModule();
  openProfilerAndDisplayProfile(
    profile,
    profilerViewMode,
    symbolicationService
  );

  Services.profiler.StopProfiler();
}

/**
 * This function is called when the profiler is started with the shortcut
 * keys, with the profiler toolbarbutton, or with the button inside the
 * popup.
 * @param {PageContext} pageContext
 */
function startProfiler(pageContext) {
  const {
    entries,
    interval,
    features,
    threads,
    duration,
  } = getRecordingSettings(pageContext, Services.profiler.GetFeatures());

  // Get the active Browser ID from browser.
  const { getActiveBrowserID } = lazy.RecordingUtils();
  const activeTabID = getActiveBrowserID();

  Services.profiler.StartProfiler(
    entries,
    interval,
    features,
    threads,
    activeTabID,
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
  if (Services.profiler.IsPaused()) {
    // The profiler is currently paused, which means that the user is already
    // attempting to capture a profile. Ignore this request.
    return;
  }
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
 * The profiler recording workflow uses two different pref paths. One set of prefs
 * is stored for local profiling, and another for remote profiling. This function
 * decides which to use. The remote prefs have ".remote" appended to the end of
 * their pref names.
 *
 * @param {PageContext} pageContext
 * @returns {PrefPostfix}
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
 * @param {string[]} objdirs
 */
function setObjdirPrefValue(objdirs) {
  Services.prefs.setCharPref(OBJDIRS_PREF, JSON.stringify(objdirs));
}

/**
 * Before Firefox 92, the objdir lists for local and remote profiling were
 * stored in separate lists. In Firefox 92 those two prefs were merged into
 * one. This function performs the migration.
 */
function migrateObjdirsPrefsIfNeeded() {
  const OLD_REMOTE_OBJDIRS_PREF = OBJDIRS_PREF + ".remote";
  const remoteString = Services.prefs.getCharPref(OLD_REMOTE_OBJDIRS_PREF, "");
  if (remoteString === "") {
    // No migration necessary.
    return;
  }

  const remoteList = JSON.parse(remoteString);
  const localList = _getArrayOfStringsPref(OBJDIRS_PREF);

  // Merge the two lists, eliminating any duplicates.
  const mergedList = [...new Set(localList.concat(remoteList))];
  setObjdirPrefValue(mergedList);
  Services.prefs.clearUserPref(OLD_REMOTE_OBJDIRS_PREF);
}

/**
 * @returns {string[]}
 */
function getObjdirPrefValue() {
  migrateObjdirsPrefsIfNeeded();
  return _getArrayOfStringsPref(OBJDIRS_PREF);
}

/**
 * @param {PageContext} pageContext
 * @param {string[]} supportedFeatures
 * @returns {RecordingSettings}
 */
function getRecordingSettings(pageContext, supportedFeatures) {
  const objdirs = getObjdirPrefValue();
  const prefPostfix = getPrefPostfix(pageContext);
  const presetName = Services.prefs.getCharPref(PRESET_PREF + prefPostfix);

  // First try to get the values from a preset. If the preset is "custom" or
  // unrecognized, getRecordingSettingsFromPreset will return null and we will
  // get the settings from individual prefs instead.
  return (
    getRecordingSettingsFromPreset(presetName, supportedFeatures, objdirs) ??
    getRecordingSettingsFromPrefs(supportedFeatures, objdirs, prefPostfix)
  );
}

/**
 * @param {string} presetName
 * @param {string[]} supportedFeatures
 * @param {string[]} objdirs
 * @return {RecordingSettings | null}
 */
function getRecordingSettingsFromPreset(
  presetName,
  supportedFeatures,
  objdirs
) {
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
    interval: preset.interval,
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
 * @param {string[]} supportedFeatures
 * @param {string[]} objdirs
 * @param {PrefPostfix} prefPostfix
 * @return {RecordingSettings}
 */
function getRecordingSettingsFromPrefs(
  supportedFeatures,
  objdirs,
  prefPostfix
) {
  // If you add a new preference here, please do not forget to update
  // `revertRecordingSettings` as well.

  const entries = Services.prefs.getIntPref(ENTRIES_PREF + prefPostfix);
  const intervalInMicroseconds = Services.prefs.getIntPref(
    INTERVAL_PREF + prefPostfix
  );
  const interval = intervalInMicroseconds / 1000;
  const features = _getArrayOfStringsPref(FEATURES_PREF + prefPostfix);
  const threads = _getArrayOfStringsPref(THREADS_PREF + prefPostfix);
  const duration = Services.prefs.getIntPref(DURATION_PREF + prefPostfix);

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
 * @param {PageContext} pageContext
 * @param {RecordingSettings} prefs
 */
function setRecordingSettings(pageContext, prefs) {
  const prefPostfix = getPrefPostfix(pageContext);
  Services.prefs.setCharPref(PRESET_PREF + prefPostfix, prefs.presetName);
  Services.prefs.setIntPref(ENTRIES_PREF + prefPostfix, prefs.entries);
  // The interval pref stores the value in microseconds for extra precision.
  const intervalInMicroseconds = prefs.interval * 1000;
  Services.prefs.setIntPref(
    INTERVAL_PREF + prefPostfix,
    intervalInMicroseconds
  );
  Services.prefs.setCharPref(
    FEATURES_PREF + prefPostfix,
    JSON.stringify(prefs.features)
  );
  Services.prefs.setCharPref(
    THREADS_PREF + prefPostfix,
    JSON.stringify(prefs.threads)
  );
  setObjdirPrefValue(prefs.objdirs);
}

const platform = AppConstants.platform;

/**
 * Revert the recording prefs for both local and remote profiling.
 * @return {void}
 */
function revertRecordingSettings() {
  for (const prefPostfix of ["", ".remote"]) {
    Services.prefs.clearUserPref(PRESET_PREF + prefPostfix);
    Services.prefs.clearUserPref(ENTRIES_PREF + prefPostfix);
    Services.prefs.clearUserPref(INTERVAL_PREF + prefPostfix);
    Services.prefs.clearUserPref(FEATURES_PREF + prefPostfix);
    Services.prefs.clearUserPref(THREADS_PREF + prefPostfix);
    Services.prefs.clearUserPref(DURATION_PREF + prefPostfix);
  }
  Services.prefs.clearUserPref(OBJDIRS_PREF);
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
  const prefPostfix = getPrefPostfix(pageContext);
  const objdirs = getObjdirPrefValue();
  let recordingSettings = getRecordingSettingsFromPreset(
    presetName,
    supportedFeatures,
    objdirs
  );

  if (!recordingSettings) {
    // No recordingSettings were found for that preset. Most likely this means this
    // is a custom preset, or it's one that we dont recognize for some reason.
    // Get the preferences from the individual preference values.
    Services.prefs.setCharPref(PRESET_PREF + prefPostfix, presetName);
    recordingSettings = getRecordingSettings(pageContext, supportedFeatures);
  }

  setRecordingSettings(pageContext, recordingSettings);
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
  getRecordingSettings,
  setRecordingSettings,
  revertRecordingSettings,
  changePreset,
  handleWebChannelMessage,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
