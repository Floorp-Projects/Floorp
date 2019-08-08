/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line with multiple entry points
 * triggers no matter which entry point we reach.
 */

add_task(
  threadFrontTest(async ({ threadFront, client, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );
    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = {
      sourceUrl: source.url,
      line: debuggee.line0 + 3,
      column: 5,
    };

    //Pause at debugger statement.
    Assert.equal(packet.frame.where.line, debuggee.line0 + 1);
    Assert.equal(packet.why.type, "debuggerStatement");

    threadFront.setBreakpoint(location, {});
    await client.waitForRequestsToSettle();

    await resume(threadFront);

    const packet2 = await waitForPause(threadFront);
    // Check the return value.
    Assert.equal(packet2.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.i, 0);
    // Check pause location
    Assert.equal(packet2.frame.where.line, debuggee.line0 + 3);
    Assert.equal(packet2.frame.where.column, 5);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location);
    await client.waitForRequestsToSettle();

    const location2 = {
      sourceUrl: source.url,
      line: debuggee.line0 + 3,
      column: 12,
    };
    threadFront.setBreakpoint(location2, {});
    await client.waitForRequestsToSettle();

    await resume(threadFront);
    const packet3 = await waitForPause(threadFront);
    // Check the return value.
    Assert.equal(packet3.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.i, 1);
    // Check execution location
    Assert.equal(packet3.frame.where.line, debuggee.line0 + 3);
    Assert.equal(packet3.frame.where.column, 12);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location2);
    await client.waitForRequestsToSettle();

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
      Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                       "debugger;\n" +                      // line0 + 1
                       "var a, i = 0;\n" +                  // line0 + 2
                       "for (i = 1; i <= 2; i++) {\n" +     // line0 + 3
                       "  a = i;\n" +                       // line0 + 4
                       "}\n",                               // line0 + 5
                       debuggee);
      /* eslint-enable */
}
