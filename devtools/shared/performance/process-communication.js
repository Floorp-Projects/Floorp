/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The following functions are used in testing to control and inspect
 * the nsIProfiler in child process content. These should be called from
 * the parent process.
 */

const FRAME_SCRIPT_UTILS_URL = "chrome://browser/content/devtools/frame-script-utils.js";
loader.lazyRequireGetter(this, "Task", "resource://gre/modules/Task.jsm", true);
loader.lazyRequireGetter(this, "uuid", "sdk/util/uuid", true);

var mm = null;

exports.consoleMethod = function (...args) {
  if (!mm) {
    throw new Error("`PMM_loadFrameScripts()` must be called before using frame scripts.");
  }

  // Terrible ugly hack -- this gets stringified when it uses the
  // message manager, so an undefined arg in `console.profileEnd()`
  // turns into a stringified "null", which is terrible. This method is only used
  // for test helpers, so swap out the argument if its undefined with an empty string.
  // Differences between empty string and undefined are tested on the front itself.
  if (args[1] == null) {
    args[1] = "";
  }
  mm.sendAsyncMessage("devtools:test:console", args);
};

exports.PMM_isProfilerActive = function () {
  return sendProfilerCommand("IsActive");
};

exports.PMM_stopProfiler = function () {
  return Task.spawn(function*() {
    let isActive = (yield sendProfilerCommand("IsActive")).isActive;
    if (isActive) {
      return sendProfilerCommand("StopProfiler");
    }
  });
};

exports.PMM_loadFrameScripts = function (gBrowser) {
  mm = gBrowser.selectedBrowser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
};

function sendProfilerCommand (method, args=[]) {
  if (!mm) {
    throw new Error("`PMM_loadFrameScripts()` must be called when using MessageManager.");
  }

  let id = uuid().toString();
  return new Promise(resolve => {
    mm.addMessageListener("devtools:test:profiler:response", handler);
    mm.sendAsyncMessage("devtools:test:profiler", { method, args, id });

    function handler ({ data }) {
      if (id !== data.id) {
        return;
      }

      mm.removeMessageListener("devtools:test:profiler:response", handler);
      resolve(data.data);
    }
  });
}
exports.sendProfilerCommand = sendProfilerCommand;
