/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line in a child script works.
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
    Assert.equal(packet.frame.where.line, debuggee.line0 + 5);
    Assert.equal(packet.why.type, "debuggerStatement");

    threadFront.setBreakpoint(location, {});
    await client.waitForRequestsToSettle();
    await resume(threadFront);

    const packet2 = await waitForPause(threadFront);
    // Check the return value.
    Assert.equal(packet2.frame.where.actor, source.actor);
    Assert.equal(packet2.frame.where.line, location.line);
    Assert.equal(packet2.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.a, 1);
    Assert.equal(debuggee.b, undefined);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location);
    await client.waitForRequestsToSettle();

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "function foo() {\n" + // line0 + 1
    "  this.a = 1;\n" +    // line0 + 2
    "  this.b = 2;\n" +    // line0 + 3
    "}\n" +                // line0 + 4
    "debugger;\n" +        // line0 + 5
    "foo();\n",            // line0 + 6
    debuggee
  );
  /* eslint-disable */
}
