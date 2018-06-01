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

const FRAME_SCRIPT_UTILS_URL = "chrome://mochitests/content/browser/devtools/client/shared/test/frame-script-utils.js";

let gMM = null;

/**
 * Loads the relevant frame scripts into the provided browser's message manager.
 */
exports.pmmLoadFrameScripts = (gBrowser) => {
  gMM = gBrowser.selectedBrowser.messageManager;
  gMM.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
};

/**
 * Clears the cached message manager.
 */
exports.pmmClearFrameScripts = () => {
  gMM = null;
};

/**
 * Sends a message to the message listener, attaching an id to the payload data.
 * Resolves a returned promise when the response is received from the message
 * listener, with the same id as part of the response payload data.
 */
exports.pmmUniqueMessage = function(message, payload) {
  if (!gMM) {
    throw new Error("`pmmLoadFrameScripts()` must be called when using MessageManager.");
  }

  const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator);
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
exports.pmmIsProfilerActive = () => {
  return exports.pmmSendProfilerCommand("IsActive");
};

/**
 * Starts the nsProfiler module.
 */
exports.pmmStartProfiler = async function({ entries, interval, features }) {
  const isActive = (await exports.pmmSendProfilerCommand("IsActive")).isActive;
  if (!isActive) {
    return exports.pmmSendProfilerCommand("StartProfiler", [entries, interval, features,
                                                            features.length]);
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
  return exports.pmmUniqueMessage("devtools:test:profiler", { method, args });
};

/**
 * Evaluates a script in content, returning a promise resolved with the
 * returned result.
 */
exports.pmmEvalInDebuggee = (script) => {
  return exports.pmmUniqueMessage("devtools:test:eval", { script });
};

/**
 * Evaluates a console method in content.
 */
exports.pmmConsoleMethod = function(method, ...args) {
  // Terrible ugly hack -- this gets stringified when it uses the
  // message manager, so an undefined arg in `console.profileEnd()`
  // turns into a stringified "null", which is terrible. This method
  // is only used for test helpers, so swap out the argument if its undefined
  // with an empty string. Differences between empty string and undefined are
  // tested on the front itself.
  if (args[0] == null) {
    args[0] = "";
  }
  return exports.pmmUniqueMessage("devtools:test:console", { method, args });
};
