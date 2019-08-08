/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that removing a breakpoint works.
 */

let done = false;

add_task(
  threadFrontTest(async ({ threadFront, client, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = { sourceUrl: source.url, line: debuggee.line0 + 2 };

    //Pause at debugger statement.
    Assert.equal(packet.frame.where.line, debuggee.line0 + 7);
    Assert.equal(packet.why.type, "debuggerStatement");

    threadFront.setBreakpoint(location, {});
    await client.waitForRequestsToSettle();

    await resume(threadFront);

    const packet2 = await waitForPause(threadFront);

    // Check the return value.
    Assert.equal(packet2.frame.where.actor, source.actorID);
    Assert.equal(packet2.frame.where.line, location.line);
    Assert.equal(packet2.why.type, "breakpoint");
    // Check that the breakpoint worked.
    Assert.equal(debuggee.a, undefined);

    // Remove the breakpoint.
    threadFront.removeBreakpoint(location);
    await client.waitForRequestsToSettle();

    done = true;
    threadFront.once("paused", function(packet) {
      // The breakpoint should not be hit again.
      threadFront.resume().then(function() {
        Assert.ok(false);
      });
    });

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
      Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                       "function foo(stop) {\n" + // line0 + 1
                       "  this.a = 1;\n" +        // line0 + 2
                       "  if (stop) return;\n" +  // line0 + 3
                       "  delete this.a;\n" +     // line0 + 4
                       "  foo(true);\n" +         // line0 + 5
                       "}\n" +                    // line0 + 6
                       "debugger;\n" +            // line1 + 7
                       "foo();\n",                // line1 + 8
                       debuggee);
      /* eslint-enable */
  if (!done) {
    Assert.ok(false);
  }
}
