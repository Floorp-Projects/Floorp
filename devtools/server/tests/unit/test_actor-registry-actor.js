/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that you can register new actors via the ActorRegistrationActor.
 */

var gClient;
var gRegistryFront;
var gActorFront;

function run_test() {
  initTestDebuggerServer();
  DebuggerServer.registerAllActors();
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(getRegistry);
  do_test_pending();
}

async function getRegistry() {
  gRegistryFront = await gClient.mainRoot.getFront("actorRegistry");
  registerNewActor();
}

function registerNewActor() {
  const options = {
    prefix: "helloActor",
    constructor: "HelloActor",
    type: { global: true },
  };

  gRegistryFront
    .registerActor("resource://test/hello-actor.js", options)
    .then(actorFront => (gActorFront = actorFront))
    .then(talkToNewActor)
    .catch(e => {
      DevToolsUtils.reportException("registerNewActor", e);
      Assert.ok(false);
    });
}

function talkToNewActor() {
  gClient.listTabs().then(({ helloActor }) => {
    Assert.ok(!!helloActor);
    gClient.request({
      to: helloActor,
      type: "hello",
    }, response => {
      Assert.ok(!response.error);
      unregisterNewActor();
    });
  });
}

function unregisterNewActor() {
  gActorFront
    .unregister()
    .then(testActorIsUnregistered)
    .catch(e => {
      DevToolsUtils.reportException("unregisterNewActor", e);
      Assert.ok(false);
    });
}

function testActorIsUnregistered() {
  gClient.listTabs().then(({ helloActor }) => {
    Assert.ok(!helloActor);

    finishClient(gClient);
  });
}
