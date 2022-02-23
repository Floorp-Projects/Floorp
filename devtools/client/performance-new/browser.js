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

const ChromeUtils = require("ChromeUtils");
const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);

const lazy = createLazyLoaders({
  Chrome: () => require("chrome"),
  Services: () => require("Services"),
  OS: () => ChromeUtils.import("resource://gre/modules/osfile.jsm"),
  PerfSymbolication: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/symbolication.jsm.js"
    ),
});

/** @type {PerformancePref["UIBaseUrl"]} */
const UI_BASE_URL_PREF = "devtools.performance.recording.ui-base-url";
/** @type {PerformancePref["UIBaseUrlPathPref"]} */
const UI_BASE_URL_PATH_PREF = "devtools.performance.recording.ui-base-url-path";

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
 * @returns {MockedExports.Browser} The browser for the opened tab.
 */
function openProfilerTab(profilerViewMode) {
  const Services = lazy.Services();
  // Find the most recently used window, as the DevTools client could be in a variety
  // of hosts.
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  if (!win) {
    throw new Error("No browser window");
  }
  const browser = win.gBrowser;
  win.focus();

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

  // We automatically open up the "full" mode if no query string is present.
  // `undefined` also means nothing is specified, and it should open the "full"
  // timeline view in that case.
  let viewModeQueryString = "";
  if (profilerViewMode === "active-tab") {
    viewModeQueryString = "?view=active-tab&implementation=js";
  } else if (profilerViewMode !== undefined && profilerViewMode !== "full") {
    viewModeQueryString = `?view=${profilerViewMode}`;
  }

  const tab = browser.addWebTab(
    `${baseUrl}${baseUrlPath}${viewModeQueryString}`,
    {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({
        userContextId: browser.contentPrincipal.userContextId,
      }),
    }
  );
  browser.selectedTab = tab;
  return tab.linkedBrowser;
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
  const Services = lazy.Services();
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
