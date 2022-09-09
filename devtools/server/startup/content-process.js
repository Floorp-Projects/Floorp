/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/process-script */

"use strict";

/*
 * Process script that listens for requests to start a `DevToolsServer` for an entire
 * content process.  Loaded into content processes by the main process during
 * content-process-connector.js' `connectToContentProcess`.
 *
 * The actual server startup itself is in a JSM so that code can be cached.
 */

function onInit(message) {
  // Only reply if we are in a real content process
  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    const { initContentProcessTarget } = ChromeUtils.importESModule(
      "resource://devtools/server/startup/content-process.sys.mjs"
    );
    initContentProcessTarget(message);
  }
}

function onClose() {
  removeMessageListener("debug:init-content-server", onInit);
  removeMessageListener("debug:close-content-server", onClose);
}

addMessageListener("debug:init-content-server", onInit);
addMessageListener("debug:close-content-server", onClose);
