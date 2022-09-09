/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("./@types/perf").Action} Action
 * @typedef {import("./@types/perf").Library} Library
 * @typedef {import("./@types/perf").PerfFront} PerfFront
 * @typedef {import("./@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("./@types/perf").RecordingState} RecordingState
 * @typedef {import("./@types/perf").SymbolicationService} SymbolicationService
 * @typedef {import("./@types/perf").PreferenceFront} PreferenceFront
 * @typedef {import("./@types/perf").PerformancePref} PerformancePref
 * @typedef {import("./@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("./@types/perf").RestartBrowserWithEnvironmentVariable} RestartBrowserWithEnvironmentVariable
 * @typedef {import("./@types/perf").GetEnvironmentVariable} GetEnvironmentVariable
 * @typedef {import("./@types/perf").GetActiveBrowserID} GetActiveBrowserID
 * @typedef {import("./@types/perf").MinimallyTypedGeckoProfile} MinimallyTypedGeckoProfile
 * * @typedef {import("./@types/perf").ProfilerViewMode} ProfilerViewMode
 */

const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);

const lazy = createLazyLoaders({
  Chrome: () => require("chrome"),
});

/** @type {PerformancePref["UIBaseUrl"]} */
const UI_BASE_URL_PREF = "devtools.performance.recording.ui-base-url";
/** @type {PerformancePref["UIBaseUrlPathPref"]} */
const UI_BASE_URL_PATH_PREF = "devtools.performance.recording.ui-base-url-path";

/** @type {PerformancePref["UIEnableActiveTabView"]} */
const UI_ENABLE_ACTIVE_TAB_PREF =
  "devtools.performance.recording.active-tab-view.enabled";

const UI_BASE_URL_DEFAULT = "https://profiler.firefox.com";
const UI_BASE_URL_PATH_DEFAULT = "/from-browser";

/**
 * This file contains all of the privileged browser-specific functionality. This helps
 * keep a clear separation between the privileged and non-privileged client code. It
 * is also helpful in being able to mock out browser behavior for tests, without
 * worrying about polluting the browser environment.
 */

/**
 * Once a profile is received from the actor, it needs to be opened up in
 * profiler.firefox.com to be analyzed. This function opens up profiler.firefox.com
 * into a new browser tab.
 * @param {ProfilerViewMode | undefined} profilerViewMode - View mode for the Firefox Profiler
 *   front-end timeline. While opening the url, we should append a query string
 *   if a view other than "full" needs to be displayed.
 * @returns {Promise<MockedExports.Browser>} The browser for the opened tab.
 */
async function openProfilerTab(profilerViewMode) {
  // Allow the user to point to something other than profiler.firefox.com.
  const baseUrl = Services.prefs.getStringPref(
    UI_BASE_URL_PREF,
    UI_BASE_URL_DEFAULT
  );
  // Allow tests to override the path.
  const baseUrlPath = Services.prefs.getStringPref(
    UI_BASE_URL_PATH_PREF,
    UI_BASE_URL_PATH_DEFAULT
  );
  // This controls whether we enable the active tab view when capturing in web
  // developer preset.
  const enableActiveTab = Services.prefs.getBoolPref(
    UI_ENABLE_ACTIVE_TAB_PREF,
    false
  );

  // We automatically open up the "full" mode if no query string is present.
  // `undefined` also means nothing is specified, and it should open the "full"
  // timeline view in that case.
  let viewModeQueryString = "";
  if (profilerViewMode === "active-tab") {
    // We're not enabling the active-tab view in all environments until we
    // iron out all its issues.
    if (enableActiveTab) {
      viewModeQueryString = "?view=active-tab&implementation=js";
    } else {
      viewModeQueryString = "?implementation=js";
    }
  } else if (profilerViewMode !== undefined && profilerViewMode !== "full") {
    viewModeQueryString = `?view=${profilerViewMode}`;
  }

  const urlToLoad = `${baseUrl}${baseUrlPath}${viewModeQueryString}`;

  // Find the most recently used window, as the DevTools client could be in a variety
  // of hosts.
  // Note that when running from the browser toolbox, there won't be the browser window,
  // but only the browser toolbox document.
  const win =
    Services.wm.getMostRecentWindow("navigator:browser") ||
    Services.wm.getMostRecentWindow("devtools:toolbox");
  if (!win) {
    throw new Error("No browser window");
  }
  win.focus();

  // The profiler frontend currently doesn't support being loaded in a private
  // window, because it does some storage writes in IndexedDB. That's why we
  // force the opening of the tab in a non-private window. This might open a new
  // non-private window if the only currently opened window is a private window.
  const contentBrowser = await new Promise(resolveOnContentBrowserCreated =>
    win.openWebLinkIn(urlToLoad, "tab", {
      forceNonPrivate: true,
      resolveOnContentBrowserCreated,
      userContextId: win.gBrowser?.contentPrincipal.userContextId,
    })
  );
  return contentBrowser;
}

/**
 * Flatten all the sharedLibraries of the different processes in the profile
 * into one list of libraries.
 * @param {MinimallyTypedGeckoProfile} profile - The profile JSON object
 * @returns {Library[]}
 */
function sharedLibrariesFromProfile(profile) {
  /**
   * @param {MinimallyTypedGeckoProfile} processProfile
   * @returns {Library[]}
   */
  function getLibsRecursive(processProfile) {
    return processProfile.libs.concat(
      ...processProfile.processes.map(getLibsRecursive)
    );
  }

  return getLibsRecursive(profile);
}

/**
 * Restarts the browser with a given environment variable set to a value.
 *
 * @type {RestartBrowserWithEnvironmentVariable}
 */
function restartBrowserWithEnvironmentVariable(envName, value) {
  const { Cc, Ci } = lazy.Chrome();
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set(envName, value);

  Services.startup.quit(
    Services.startup.eForceQuit | Services.startup.eRestart
  );
}

/**
 * Gets an environment variable from the browser.
 *
 * @type {GetEnvironmentVariable}
 */
function getEnvironmentVariable(envName) {
  const { Cc, Ci } = lazy.Chrome();
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  return env.get(envName);
}

/**
 * @param {Window} window
 * @param {string[]} objdirs
 * @param {(objdirs: string[]) => unknown} changeObjdirs
 */
function openFilePickerForObjdir(window, objdirs, changeObjdirs) {
  const { Cc, Ci } = lazy.Chrome();
  const FilePicker = Cc["@mozilla.org/filepicker;1"].createInstance(
    Ci.nsIFilePicker
  );
  FilePicker.init(window, "Pick build directory", FilePicker.modeGetFolder);
  FilePicker.open(rv => {
    if (rv == FilePicker.returnOK) {
      const path = FilePicker.file.path;
      if (path && !objdirs.includes(path)) {
        const newObjdirs = [...objdirs, path];
        changeObjdirs(newObjdirs);
      }
    }
  });
}

module.exports = {
  openProfilerTab,
  sharedLibrariesFromProfile,
  restartBrowserWithEnvironmentVariable,
  getEnvironmentVariable,
  openFilePickerForObjdir,
};
