/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../client/shared/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/debugger-server");

async function createLocalClient() {
  // Instantiate a minimal server
  DebuggerServer.init();
  DebuggerServer.allowChromeProcess = true;
  if (!DebuggerServer.createRootActor) {
    DebuggerServer.registerAllActors();
  }
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();
  return client;
}
