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
 * @typedef {import("./@types/perf").GetSymbolTableCallback} GetSymbolTableCallback
 * @typedef {import("./@types/perf").PreferenceFront} PreferenceFront
 * @typedef {import("./@types/perf").PerformancePref} PerformancePref
 * @typedef {import("./@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 * @typedef {import("./@types/perf").RestartBrowserWithEnvironmentVariable} RestartBrowserWithEnvironmentVariable
 * @typedef {import("./@types/perf").GetEnvironmentVariable} GetEnvironmentVariable
 * @typedef {import("./@types/perf").GetActiveBrowsingContextID} GetActiveBrowsingContextID
 * @typedef {import("./@types/perf").MinimallyTypedGeckoProfile} MinimallyTypedGeckoProfile
 */

const ChromeUtils = require("ChromeUtils");
const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);

const lazy = createLazyLoaders({
  Chrome: () => require("chrome"),
  Services: () => require("Services"),
  OS: () => ChromeUtils.import("resource://gre/modules/osfile.jsm"),
  ProfilerGetSymbols: () =>
    ChromeUtils.import("resource://gre/modules/ProfilerGetSymbols.jsm"),
  PerfSymbolication: () =>
    ChromeUtils.import(
      "resource://devtools/client/performance-new/symbolication.jsm.js"
    ),
});

const TRANSFER_EVENT = "devtools:perf-html-transfer-profile";
const SYMBOL_TABLE_REQUEST_EVENT = "devtools:perf-html-request-symbol-table";
const SYMBOL_TABLE_RESPONSE_EVENT = "devtools:perf-html-reply-symbol-table";

/** @type {PerformancePref["UIBaseUrl"]} */
const UI_BASE_URL_PREF = "devtools.performance.recording.ui-base-url";
/** @type {PerformancePref["UIBaseUrlPathPref"]} */
const UI_BASE_URL_PATH_PREF = "devtools.performance.recording.ui-base-url-path";

const UI_BASE_URL_DEFAULT = "https://profiler.firefox.com";
const UI_BASE_URL_PATH_DEFAULT = "/from-addon";

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
 * @param {MinimallyTypedGeckoProfile} profile - The Gecko profile.
 * @param {GetSymbolTableCallback} getSymbolTableCallback - A callback function with the signature
 *   (debugName, breakpadId) => Promise<SymbolTableAsTuple>, which will be invoked
 *   when profiler.firefox.com sends SYMBOL_TABLE_REQUEST_EVENT messages to us. This
 *   function should obtain a symbol table for the requested binary and resolve the
 *   returned promise with it.
 */
function receiveProfile(profile, getSymbolTableCallback) {
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

  const tab = browser.addWebTab(`${baseUrl}${baseUrlPath}`, {
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
        // Re-wrap the error object into an object that is Structured Clone-able.
        const { name, message, lineNumber, fileName } = error;
        mm.sendAsyncMessage(SYMBOL_TABLE_RESPONSE_EVENT, {
          status: "error",
          debugName,
          breakpadId,
          error: { name, message, lineNumber, fileName },
        });
      }
    );
  });
}

/**
 * Returns a function getDebugPathFor(debugName, breakpadId) => Library which
 * resolves a (debugName, breakpadId) pair to the library's information, which
 * contains the absolute paths on the file system where the binary and its
 * optional pdb file are stored.
 *
 * This is needed for the following reason:
 *  - In order to obtain a symbol table for a system library, we need to know
 *    the library's absolute path on the file system. On Windows, we
 *    additionally need to know the absolute path to the library's PDB file,
 *    which we call the binary's "debugPath".
 *  - Symbol tables are requested asynchronously, by the profiler UI, after the
 *    profile itself has been obtained.
 *  - When the symbol tables are requested, we don't want the profiler UI to
 *    pass us arbitrary absolute file paths, as an extra defense against
 *    potential information leaks.
 *  - Instead, when the UI requests symbol tables, it identifies the library
 *    with a (debugName, breakpadId) pair. We need to map that pair back to the
 *    absolute paths.
 *  - We get the "trusted" paths from the "libs" sections of the profile. We
 *    trust these paths because we just obtained the profile directly from
 *    Gecko.
 *  - This function builds the (debugName, breakpadId) => Library mapping and
 *    retains it on the returned closure so that it can be consulted after the
 *    profile has been passed to the UI.
 *
 * @param {MinimallyTypedGeckoProfile} profile - The profile JSON object
 * @returns {(debugName: string, breakpadId: string) => Library | undefined}
 */
function createLibraryMap(profile) {
  const map = new Map();

  /**
   * @param {MinimallyTypedGeckoProfile} processProfile
   */
  function fillMapForProcessRecursive(processProfile) {
    for (const lib of processProfile.libs) {
      const { debugName, breakpadId } = lib;
      const key = [debugName, breakpadId].join(":");
      map.set(key, lib);
    }
    for (const subprocess of processProfile.processes) {
      fillMapForProcessRecursive(subprocess);
    }
  }

  fillMapForProcessRecursive(profile);
  return function getLibraryFor(debugName, breakpadId) {
    const key = [debugName, breakpadId].join(":");
    return map.get(key);
  };
}

/**
 * Return a function `getSymbolTable` that calls getSymbolTableMultiModal with the
 * right arguments.
 *
 * @param {MinimallyTypedGeckoProfile} profile - The raw profie (not gzipped).
 * @param {() => string[]} getObjdirs - A function that returns an array of objdir paths
 *   on the host machine that should be searched for relevant build artifacts.
 * @param {PerfFront} perfFront
 * @return {GetSymbolTableCallback}
 */
function createMultiModalGetSymbolTableFn(profile, getObjdirs, perfFront) {
  const libraryGetter = createLibraryMap(profile);

  return async function getSymbolTable(debugName, breakpadId) {
    const lib = libraryGetter(debugName, breakpadId);
    if (!lib) {
      throw new Error(
        `Could not find the library for "${debugName}", "${breakpadId}".`
      );
    }
    const objdirs = getObjdirs();
    const { getSymbolTableMultiModal } = lazy.PerfSymbolication();
    return getSymbolTableMultiModal(lib, objdirs, perfFront);
  };
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
  receiveProfile,
  createMultiModalGetSymbolTableFn,
  restartBrowserWithEnvironmentVariable,
  getEnvironmentVariable,
  openFilePickerForObjdir,
};
