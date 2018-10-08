/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check target-scoped actor lifetimes.
 */

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const ACTORS_URL = EXAMPLE_URL + "testactors.js";
const TAB_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

add_task(async function() {
  await addTab(TAB_URL);

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  await registerActorInContentProcess(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { target: true },
  });

  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  const [type] = await client.connect();
  is(type, "browser",
    "Root actor should identify itself as a browser.");

  const [grip] = await attachTargetActorForUrl(client, TAB_URL);
  await testTargetScopedActor(client, grip);
  await closeTab(client, grip);
  await client.close();
});

async function testTargetScopedActor(client, grip) {
  ok(grip.testOneActor,
    "Found the test target-scoped actor.");
  ok(grip.testOneActor.includes("testOne"),
    "testOneActor's actorPrefix should be used.");

  const response = await client.request({ to: grip.testOneActor, type: "ping" });
  is(response.pong, "pong",
     "Actor should respond to requests.");
}

async function closeTab(client, grip) {
  await removeTab(gBrowser.selectedTab);
  await Assert.rejects(
    client.request({ to: grip.testOneActor, type: "ping" }),
    err => err.message === `'ping' active request packet to '${grip.testOneActor}' ` +
                           `can't be sent as the connection just closed.`,
    "testOneActor went away.");
}

async function attachTargetActorForUrl(client, url) {
  const grip = await getTargetActorForUrl(client, url);
  const [ response ] = await client.attachTarget(grip.actor);
  return [grip, response];
}

function getTargetActorForUrl(client, url) {
  const deferred = promise.defer();

  client.listTabs().then(response => {
    const targetActor = response.tabs.filter(grip => grip.url == url).pop();
    deferred.resolve(targetActor);
  });

  return deferred.promise;
}
