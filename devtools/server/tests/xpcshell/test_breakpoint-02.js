/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

add_task(
  threadFrontTest(async ({ threadFront, client, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );
    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = { sourceUrl: source.url, line: debuggee.line0 + 3 };

    //Pause at debugger statement.
    Assert.equal(packet.frame.where.line, debuggee.line0 + 1);
    Assert.equal(packet.why.type, "debuggerStatement");

    await threadFront.resume();

    // Setting the breakpoint later should interrupt the debuggee.
    threadFront.once("paused", function(packet) {
      Assert.equal(packet.why.type, "interrupted");
    });

    threadFront.setBreakpoint(location, {});
    await client.waitForRequestsToSettle();
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "debugger;\n" +
    "var a = 1;\n" +  // line0 + 2
    "var b = 2;\n",  // line0 + 3
    debuggee
  );
  /* eslint-disable */
}
