/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * The following functions are used in testing to control and inspect
 * the nsIProfiler in child process content. These should be called from
 * the parent process.
 */

let gSelectedBrowser = null;

/**
 * Loads the relevant frame scripts into the provided browser's message manager.
 */
exports.pmmInitWithBrowser = gBrowser => {
  gSelectedBrowser = gBrowser.selectedBrowser;
};

/**
 * Checks if the nsProfiler module is active.
 */
exports.pmmIsProfilerActive = () => {
  return exports.pmmSendProfilerCommand("IsActive");
};

/**
 * Starts the nsProfiler module.
 */
exports.pmmStartProfiler = async function({ entries, interval, features }) {
  const isActive = (await exports.pmmSendProfilerCommand("IsActive")).isActive;
  if (!isActive) {
    return exports.pmmSendProfilerCommand("StartProfiler", [
      entries,
      interval,
      features,
      features.length,
    ]);
  }
  return null;
};
/**
 * Stops the nsProfiler module.
 */
exports.pmmStopProfiler = async function() {
  const isActive = (await exports.pmmSendProfilerCommand("IsActive")).isActive;
  if (isActive) {
    return exports.pmmSendProfilerCommand("StopProfiler");
  }
  return null;
};

/**
 * Calls a method on the nsProfiler module.
 */
exports.pmmSendProfilerCommand = (method, args = []) => {
  // This script is loaded via the CommonJS module so the global
  // SpecialPowers isn't available, so get it from the browser's window.
  return gSelectedBrowser.ownerGlobal.SpecialPowers.spawn(
    gSelectedBrowser,
    [method, args],
    (methodChild, argsChild) => {
      return Services.profiler[methodChild](...argsChild);
    }
  );
};

/**
 * Evaluates a console method in content.
 */
exports.pmmConsoleMethod = function(method, ...args) {
  // This script is loaded via the CommonJS module so the global
  // SpecialPowers isn't available, so get it from the browser's window.
  return gSelectedBrowser.ownerGlobal.SpecialPowers.spawn(
    gSelectedBrowser,
    [method, args],
    (methodChild, argsChild) => {
      content.console[methodChild].apply(content.console, argsChild);
    }
  );
};
