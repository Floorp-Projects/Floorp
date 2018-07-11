/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check target-scoped actor lifetimes.
 */

const ACTORS_URL = CHROME_URL + "testactors.js";
const TAB_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

var gClient;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  DebuggerServer.registerModule(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { target: true },
  });

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then(() => attachTargetActorForUrl(gClient, TAB_URL))
      .then(testTargetScopedActor)
      .then(closeTab)
      .then(() => gClient.close())
      .then(finish)
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testTargetScopedActor([aGrip, aResponse]) {
  let deferred = promise.defer();

  ok(aGrip.testOneActor,
    "Found the test target-scoped actor.");
  ok(aGrip.testOneActor.includes("testOne"),
    "testOneActor's actorPrefix should be used.");

  gClient.request({ to: aGrip.testOneActor, type: "ping" }, aResponse => {
    is(aResponse.pong, "pong",
      "Actor should respond to requests.");

    deferred.resolve(aResponse.actor);
  });

  return deferred.promise;
}

function closeTab(aTestActor) {
  return removeTab(gBrowser.selectedTab).then(() => {
    let deferred = promise.defer();

    try {
      gClient.request({ to: aTestActor, type: "ping" }, aResponse => {
        ok(false, "testTargetScopedActor1 didn't go away with the tab.");
        deferred.reject(aResponse);
      });
    } catch (e) {
      is(e.message, "'ping' request packet has no destination.", "testOnActor went away.");
      deferred.resolve();
    }

    return deferred.promise;
  });
}

registerCleanupFunction(function () {
  gClient = null;
});
