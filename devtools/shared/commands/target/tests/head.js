/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");
const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");

async function createLocalClient() {
  // Instantiate a minimal server
  DevToolsServer.init();
  DevToolsServer.allowChromeProcess = true;
  if (!DevToolsServer.createRootActor) {
    DevToolsServer.registerAllActors();
  }
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();
  return client;
}
