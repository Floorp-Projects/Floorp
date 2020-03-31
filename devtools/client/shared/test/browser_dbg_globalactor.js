/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added global actor API.
 */

"use strict";

var { DevToolsServer } = require("devtools/server/devtools-server");
var { ActorRegistry } = require("devtools/server/actors/utils/actor-registry");
var { DevToolsClient } = require("devtools/client/devtools-client");

const ACTORS_URL = EXAMPLE_URL + "testactors.js";

add_task(async function() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  ActorRegistry.registerModule(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { global: true },
  });

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  let response = await client.mainRoot.rootForm;
  const globalActor = response.testOneActor;
  ok(globalActor, "Found the test global actor.");
  ok(
    globalActor.includes("testOne"),
    "testGlobalActor1's typeName should be used."
  );

  response = await client.request({ to: globalActor, type: "ping" });
  is(response.pong, "pong", "Actor should respond to requests.");

  // Send another ping to see if the same actor is used.
  response = await client.request({ to: globalActor, type: "ping" });
  is(response.pong, "pong", "Actor should respond to requests.");

  // Make sure that lazily-created actors are created only once.
  let count = 0;
  for (const connID of Object.getOwnPropertyNames(
    DevToolsServer._connections
  )) {
    const conn = DevToolsServer._connections[connID];
    const computedPrefix = conn._prefix + "testOne";
    for (const pool of conn._extraPools) {
      for (const actor of pool.poolChildren()) {
        if (actor.actorID.startsWith(computedPrefix)) {
          count++;
        }
      }
    }
  }

  is(count, 1, "Only one actor exists in all pools. One global actor.");

  await client.close();
});
