/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check extension-added tab actor lifetimes.
 */

const ACTORS_URL = CHROME_URL + "testactors.js";
const TAB_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

var gClient;

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  DebuggerServer.addActors(ACTORS_URL);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then(() => attachTabActorForUrl(gClient, TAB_URL))
      .then(testTabActor)
      .then(closeTab)
      .then(() => gClient.close())
      .then(finish)
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testTabActor([aGrip, aResponse]) {
  let deferred = promise.defer();

  ok(aGrip.testTabActor1,
    "Found the test tab actor.");
  ok(aGrip.testTabActor1.includes("test_one"),
    "testTabActor1's actorPrefix should be used.");

  gClient.request({ to: aGrip.testTabActor1, type: "ping" }, aResponse => {
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
        ok(false, "testTabActor1 didn't go away with the tab.");
        deferred.reject(aResponse);
      });
    } catch (e) {
      is(e.message, "'ping' request packet has no destination.", "testTabActor1 went away.");
      deferred.resolve();
    }

    return deferred.promise;
  });
}

registerCleanupFunction(function () {
  gClient = null;
});
