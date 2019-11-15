/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that logpoints generate console messages.
 */

var gDebuggee;
var gClient;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      gClient = client;
      test_simple_breakpoint();
    },
    { waitForFinish: true }
  )
);

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

    // Set a logpoint which should invoke console.log.
    gThreadFront.setBreakpoint(
      {
        sourceUrl: source.url,
        line: 3,
      },
      { logValue: "a" }
    );
    await gClient.waitForRequestsToSettle();

    // Execute the rest of the code.
    await gThreadFront.resume();
    Assert.equal(lastMessage.level, "logPoint");
    Assert.equal(lastMessage.arguments[0], "three");
    threadFrontTestFinished();
  });

  /* eslint-disable */
  Cu.evalInSandbox("debugger;\n" + // 1
                   "var a = 'three';\n" +  // 2
                   "var b = 2;\n", // 3
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */
}
