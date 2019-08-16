/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that logpoints generate console errors if the logpoint statement is invalid.
 */

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-logpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-logpoint", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      test_simple_breakpoint();
    });
  });
  do_test_pending();
}

function test_simple_breakpoint() {
  const rootActor = gClient.transport._serverConnection.rootActor;
  const threadActor =
    rootActor._parameters.tabList._targetActors[0].threadActor;

  let lastMessage;
  threadActor._parent._consoleActor = {
    onConsoleAPICall(message) {
      lastMessage = message;
    },
  };

  gThreadFront.once("paused", async function(packet) {
    const source = await getSourceById(gThreadFront, packet.frame.where.actor);

    // Set a logpoint which should throw an error message.
    await gThreadFront.setBreakpoint(
      {
        sourceUrl: source.url,
        line: 3,
      },
      { logValue: "c" }
    );

    // Execute the rest of the code.
    await gThreadFront.resume();
    Assert.equal(lastMessage.level, "logPointError");
    Assert.equal(lastMessage.arguments[0], "c is not defined");
    finishClient(gClient);
  });

  /* eslint-disable */
  Cu.evalInSandbox(
    "debugger;\n" + // 1
    "var a = 'three';\n" + // 2
      "var b = 2;\n", // 3
    gDebuggee,
    "1.8",
    "test.js",
    1
  );
  /* eslint-enable */
}
