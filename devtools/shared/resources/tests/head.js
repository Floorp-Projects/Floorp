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

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

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

async function initResourceWatcherAndTarget(tab) {
  const { TargetList } = require("devtools/shared/resources/target-list");
  const {
    ResourceWatcher,
  } = require("devtools/shared/resources/resource-watcher");

  // Create a TargetList for the test tab
  const client = await createLocalClient();

  let target;
  if (tab) {
    target = await client.mainRoot.getTab({ tab });
  } else {
    const descriptor = await client.mainRoot.getMainProcess();
    target = await descriptor.getTarget();
  }

  const targetList = new TargetList(client.mainRoot, target);
  await targetList.startListening();

  // Now create a ResourceWatcher
  const resourceWatcher = new ResourceWatcher(targetList);

  return { client, resourceWatcher, targetList };
}
