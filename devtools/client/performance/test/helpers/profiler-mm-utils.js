/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * The following functions are used in testing to control and inspect
 * the nsIProfiler in child process content. These should be called from
 * the parent process.
 */

const { Cc, Ci } = require("chrome");
const { Task } = require("devtools/shared/task");

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js";

let gMM = null;

/**
 * Loads the relevant frame scripts into the provided browser's message manager.
 */
exports.PMM_loadFrameScripts = (gBrowser) => {
  gMM = gBrowser.selectedBrowser.messageManager;
  gMM.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
};

/**
 * Clears the cached message manager.
 */
exports.PMM_clearFrameScripts = () => {
  gMM = null;
};

/**
 * Sends a message to the message listener, attaching an id to the payload data.
 * Resolves a returned promise when the response is received from the message
 * listener, with the same id as part of the response payload data.
 */
exports.PMM_uniqueMessage = function (message, payload) {
  if (!gMM) {
    throw new Error("`PMM_loadFrameScripts()` must be called when using MessageManager.");
  }

  let { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  payload.id = generateUUID().toString();

  return new Promise(resolve => {
    gMM.addMessageListener(message + ":response", function onHandler({ data }) {
      if (payload.id == data.id) {
        gMM.removeMessageListener(message + ":response", onHandler);
        resolve(data.data);
      }
    });
    gMM.sendAsyncMessage(message, payload);
  });
};

/**
 * Checks if the nsProfiler module is active.
 */
exports.PMM_isProfilerActive = () => {
  return exports.PMM_sendProfilerCommand("IsActive");
};

/**
 * Starts the nsProfiler module.
 */
exports.PMM_startProfiler = Task.async(function* ({ entries, interval, features }) {
  let isActive = (yield exports.PMM_sendProfilerCommand("IsActive")).isActive;
  if (!isActive) {
    return exports.PMM_sendProfilerCommand("StartProfiler", [entries, interval, features, features.length]);
  }
});
/**
 * Stops the nsProfiler module.
 */
exports.PMM_stopProfiler = Task.async(function* () {
  let isActive = (yield exports.PMM_sendProfilerCommand("IsActive")).isActive;
  if (isActive) {
    return exports.PMM_sendProfilerCommand("StopProfiler");
  }
});

/**
 * Calls a method on the nsProfiler module.
 */
exports.PMM_sendProfilerCommand = (method, args = []) => {
  return exports.PMM_uniqueMessage("devtools:test:profiler", { method, args });
};

/**
 * Evaluates a script in content, returning a promise resolved with the
 * returned result.
 */
exports.PMM_evalInDebuggee = (script) => {
  return exports.PMM_uniqueMessage("devtools:test:eval", { script });
};

/**
 * Evaluates a console method in content.
 */
exports.PMM_consoleMethod = function (method, ...args) {
  // Terrible ugly hack -- this gets stringified when it uses the
  // message manager, so an undefined arg in `console.profileEnd()`
  // turns into a stringified "null", which is terrible. This method
  // is only used for test helpers, so swap out the argument if its undefined
  // with an empty string. Differences between empty string and undefined are
  // tested on the front itself.
  if (args[0] == null) {
    args[0] = "";
  }
  return exports.PMM_uniqueMessage("devtools:test:console", { method, args });
};
