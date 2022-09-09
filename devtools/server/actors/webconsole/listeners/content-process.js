/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Process script used to forward console calls from content processes to parent process
const CONTENT_PROCESS_SCRIPT =
  "resource://devtools/server/actors/webconsole/content-process-forward.js";

/**
 * Forward console message calls from content processes to the parent process.
 * Used by non-multiprocess Browser Console and Browser Toolbox Console to see messages
 * from all processes.
 *
 * @constructor
 * @param Function handler
 *        This function is invoked with one argument, the message that was forwarded from
 *        the content process to the parent process.
 */
class ContentProcessListener {
  constructor(handler) {
    this.handler = handler;

    Services.ppmm.addMessageListener("Console:Log", this);
    Services.ppmm.loadProcessScript(CONTENT_PROCESS_SCRIPT, true);
  }

  receiveMessage(message) {
    const logMsg = message.data;
    logMsg.wrappedJSObject = logMsg;
    this.handler(logMsg);
  }

  destroy() {
    // Tell the content processes to stop listening and forwarding messages
    Services.ppmm.broadcastAsyncMessage(
      "DevTools:StopForwardingContentProcessMessage"
    );

    Services.ppmm.removeMessageListener("Console:Log", this);
    Services.ppmm.removeDelayedProcessScript(CONTENT_PROCESS_SCRIPT);

    this.handler = null;
  }
}

exports.ContentProcessListener = ContentProcessListener;
