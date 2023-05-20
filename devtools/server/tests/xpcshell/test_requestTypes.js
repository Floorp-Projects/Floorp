/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { rootSpec } = require("resource://devtools/shared/specs/root.js");
const {
  generateRequestTypes,
} = require("resource://devtools/shared/protocol/Actor.js");

add_task(async function () {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();

  const response = await client.mainRoot.requestTypes();
  const expectedRequestTypes = Object.keys(generateRequestTypes(rootSpec));

  Assert.ok(Array.isArray(response.requestTypes));
  Assert.equal(
    JSON.stringify(response.requestTypes),
    JSON.stringify(expectedRequestTypes)
  );

  await client.close();
});
