/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that conditions are respected when specified in a logpoint.
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
        line: 4,
      },
      { logValue: "a", condition: "a === 5" }
    );
    await gClient.waitForRequestsToSettle();

    // Execute the rest of the code.
    await gThreadFront.resume();
    Assert.equal(lastMessage.arguments[0], 5);
    threadFrontTestFinished();
  });

  /* eslint-disable */
  Cu.evalInSandbox("debugger;\n" + // 1
                   "var a = 1;\n" +  // 2
                   "while (a < 10) {\n" + // 3
                   "  a++;\n" + // 4
                   "}",
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */
}
