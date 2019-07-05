/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

// Process script used to forward console calls from content processes to parent process
const CONTENT_PROCESS_SCRIPT =
  "resource://devtools/server/actors/webconsole/content-process-forward.js";

/**
 * Forward console message calls from content processes to the parent process.
 * Used by Browser console and toolbox to see messages from all processes.
 *
 * @constructor
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onConsoleAPICall(message)
 */
function ContentProcessListener(listener) {
  this.listener = listener;

  Services.ppmm.addMessageListener("Console:Log", this);
  Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
}

exports.ContentProcessListener = ContentProcessListener;

ContentProcessListener.prototype = {
  receiveMessage(message) {
    const logMsg = message.data;
    logMsg.wrappedJSObject = logMsg;
    this.listener.onConsoleAPICall(logMsg);
  },

  destroy() {
    // Tell the content processes to stop listening and forwarding messages
    Services.ppmm.broadcastAsyncMessage(
      "DevTools:StopForwardingContentProcessMessage"
    );

    Services.ppmm.removeMessageListener("Console:Log", this);
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);

    this.listener = null;
  },
};
