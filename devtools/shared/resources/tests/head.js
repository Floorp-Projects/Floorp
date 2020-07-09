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

  let descriptor;
  if (tab) {
    descriptor = await client.mainRoot.getTab({ tab });
  } else {
    descriptor = await client.mainRoot.getMainProcess();
  }

  const target = await descriptor.getTarget();
  const targetList = new TargetList(client.mainRoot, target);
  await targetList.startListening();

  // Now create a ResourceWatcher
  const resourceWatcher = new ResourceWatcher(targetList);

  return { client, resourceWatcher, targetList };
}

// Copied from devtools/shared/webconsole/test/chrome/common.js
function checkObject(object, expected) {
  if (object && object.getGrip) {
    object = object.getGrip();
  }

  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue);
  }
}

function checkValue(name, value, expected) {
  if (expected === null) {
    is(value, null, `'${name}' is null`);
  } else if (value === undefined) {
    is(value, undefined, `'${name}' is undefined`);
  } else if (value === null) {
    is(value, expected, `'${name}' has expected value`);
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'");
  } else if (expected instanceof RegExp) {
    ok(expected.test(value), name + ": " + expected + " matched " + value);
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'");
    ok(Array.isArray(value), `property '${name}' is an array`);

    is(value.length, expected.length, "Array has expected length");
    if (value.length !== expected.length) {
      is(JSON.stringify(value, null, 2), JSON.stringify(expected, null, 2));
    } else {
      checkObject(value, expected);
    }
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'");
    checkObject(value, expected);
  }
}
