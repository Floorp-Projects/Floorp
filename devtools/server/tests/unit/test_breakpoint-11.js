/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that setting a breakpoint in a line with bytecodes in multiple
 * scripts, sets the breakpoint in all of them (bug 793214).
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
      line: debuggee.line0 + 2,
      column: 8,
    };

    //Pause at debugger statement.
    Assert.equal(packet.frame.where.line, debuggee.line0 + 1);
    Assert.equal(packet.why.type, "debuggerStatement");

    threadFront.setBreakpoint(location, {});
    await resume(threadFront);

    const packet2 = await waitForPause(threadFront);

    // Check the return value.
    Assert.equal(packet2.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.a, undefined);
    // Check execution location
    Assert.equal(packet2.frame.where.line, debuggee.line0 + 2);
    Assert.equal(packet2.frame.where.column, 8);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location);

    const location2 = {
      sourceUrl: source.url,
      line: debuggee.line0 + 2,
      column: 32,
    };
    threadFront.setBreakpoint(location2, {});

    await resume(threadFront);
    const packet3 = await waitForPause(threadFront);

    // Check the return value.
    Assert.equal(packet3.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.a.b, 1);
    Assert.equal(debuggee.res, undefined);
    // Check execution location
    Assert.equal(packet3.frame.where.line, debuggee.line0 + 2);
    Assert.equal(packet3.frame.where.column, 32);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location2);

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
      Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                       "debugger;\n" +                      // line0 + 1
                       "var a = { b: 1, f: function() { return 2; } };\n" + // line0+2
                       "var res = a.f();\n",               // line0 + 3
                       debuggee);
      /* eslint-enable */
}
