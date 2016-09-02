/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the break-on-dom-events request works for load event listeners.
 */

const TAB_URL = EXAMPLE_URL + "doc_event-listeners-04.html";

var gClient, gThreadClient;

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
      .then(() => attachThreadActorForUrl(gClient, TAB_URL))
      .then(aThreadClient => gThreadClient = aThreadClient)
      .then(pauseDebuggee)
      .then(testBreakOnLoad)
      .then(() => gClient.close())
      .then(finish)
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function pauseDebuggee() {
  let deferred = promise.defer();

  gClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.type, "paused",
      "We should now be paused.");
    is(aPacket.why.type, "debuggerStatement",
      "The debugger statement was hit.");

    gThreadClient.resume(deferred.resolve);
  });

  // Spin the event loop before causing the debuggee to pause, to allow
  // this function to return first.
  executeSoon(() => triggerButtonClick());

  return deferred.promise;
}

// Test pause on a load event.
function testBreakOnLoad() {
  let deferred = promise.defer();

  // Test calling pauseOnDOMEvents from a running state.
  gThreadClient.pauseOnDOMEvents(["load"], (aPacket) => {
    is(aPacket.error, undefined,
      "The pause-on-load request completed successfully.");
    let handlers = ["loadHandler"];

    gClient.addListener("paused", function tester(aEvent, aPacket) {
      is(aPacket.why.type, "pauseOnDOMEvents",
        "A hidden breakpoint was hit.");

      is(aPacket.frame.where.line, 15, "Found the load event listener.");
      gClient.removeListener("paused", tester);
      deferred.resolve();

      gThreadClient.resume(() => triggerButtonClick(handlers.slice(-1)));
    });

    getTabActorForUrl(gClient, TAB_URL).then(aGrip => {
      gClient.attachTab(aGrip.actor, (aResponse, aTabClient) => {
        aTabClient.reload();
      });
    });
  });

  return deferred.promise;
}

function triggerButtonClick() {
  let button = content.document.querySelector("button");
  EventUtils.sendMouseEvent({ type: "click" }, button);
}

registerCleanupFunction(function () {
  gClient = null;
  gThreadClient = null;
});
