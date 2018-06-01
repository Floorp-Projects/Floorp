/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global addMessageListener, removeMessageListener */

"use strict";

/*
 * Process script that listens for requests to start a `DebuggerServer` for an entire
 * content process.  Loaded into content processes by the main process during
 * `DebuggerServer.connectToContentProcess`.
 *
 * The actual server startup itself is in a JSM so that code can be cached.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

function onInit(message) {
  // Only reply if we are in a real content process
  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    const {init} = ChromeUtils.import("resource://devtools/server/startup/content-process.jsm", {});
    init(message);
  }
}

function onClose() {
  removeMessageListener("debug:init-content-server", onInit);
  removeMessageListener("debug:close-content-server", onClose);
}

addMessageListener("debug:init-content-server", onInit);
addMessageListener("debug:close-content-server", onClose);
