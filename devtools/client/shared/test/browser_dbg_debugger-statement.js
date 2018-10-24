/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behavior of the debugger statement.
 */

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB_URL = TEST_URI_ROOT + "doc_inline-debugger-statement.html";

var gClient;
var gTab;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(([aType, aTraits]) => {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");

    addTab(TAB_URL)
      .then(tab => {
        gTab = tab;
        return attachTargetActorForUrl(gClient, TAB_URL);
      })
      .then(testEarlyDebuggerStatement)
      .then(testDebuggerStatement)
      .then(() => gClient.close())
      .then(finish)
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });
  });
}

function testEarlyDebuggerStatement([aGrip, aResponse]) {
  const deferred = getDeferredPromise().defer();

  const onPaused = function(event, packet) {
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
  const deferred = getDeferredPromise().defer();

  gClient.addListener("paused", (event, packet) => {
    gClient.request({ to: aResponse.threadActor, type: "resume" }, () => {
      ok(true, "The pause handler was triggered on a debugger statement.");
      deferred.resolve();
    });
  });

  // Reach around the debugging protocol and execute the debugger statement.
  callInTab(gTab, "runDebuggerStatement");

  return deferred.promise;
}

registerCleanupFunction(function() {
  gClient = null;
});
