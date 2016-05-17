/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the behavior of the debugger statement.
 */

const TAB_URL = EXAMPLE_URL + "doc_inline-debugger-statement.html";

var gClient;
var gTab;

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then((aTab) => {
        gTab = aTab;
        return attachTabActorForUrl(gClient, TAB_URL);
      })
      .then(testEarlyDebuggerStatement)
      .then(testDebuggerStatement)
      .then(closeConnection)
      .then(finish)
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testEarlyDebuggerStatement([aGrip, aResponse]) {
  let deferred = promise.defer();

  let onPaused = function (aEvent, aPacket) {
    ok(false, "Pause shouldn't be called before we've attached!");
    deferred.reject();
  };

  gClient.addListener("paused", onPaused);

  // This should continue without nesting an event loop and calling
  // the onPaused hook, because we haven't attached yet.
  callInTab(gTab, "runDebuggerStatement");

  gClient.removeListener("paused", onPaused);

  // Now attach and resume...
  gClient.request({ to: aResponse.threadActor, type: "attach" }, () => {
    gClient.request({ to: aResponse.threadActor, type: "resume" }, () => {
      ok(true, "Pause wasn't called before we've attached.");
      deferred.resolve([aGrip, aResponse]);
    });
  });

  return deferred.promise;
}

function testDebuggerStatement([aGrip, aResponse]) {
  let deferred = promise.defer();

  gClient.addListener("paused", (aEvent, aPacket) => {
    gClient.request({ to: aResponse.threadActor, type: "resume" }, () => {
      ok(true, "The pause handler was triggered on a debugger statement.");
      deferred.resolve();
    });
  });

  // Reach around the debugging protocol and execute the debugger statement.
  callInTab(gTab, "runDebuggerStatement");

  return deferred.promise;
}

function closeConnection() {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function () {
  gClient = null;
});
