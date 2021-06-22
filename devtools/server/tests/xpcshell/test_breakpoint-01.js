/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic breakpoint functionality.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    info("Wait for the debugger statement to be hit");
    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );
    const source = await getSourceById(threadFront, packet1.frame.where.actor);
    const location = { sourceUrl: source.url, line: debuggee.line0 + 3 };

    threadFront.setBreakpoint(location, {});

    await threadFront.resume();
    const packet2 = await waitForPause(threadFront);

    info("Paused at the breakpoint");
    Assert.equal(packet2.frame.where.actor, source.actor);
    Assert.equal(packet2.frame.where.line, location.line);
    Assert.equal(packet2.why.type, "breakpoint");

    info("Check that the breakpoint worked.");
    Assert.equal(debuggee.a, 1);
    Assert.equal(debuggee.b, undefined);

    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  /*
   * Be sure to run debuggee code in its own HTML 'task', so that when we call
   * the onDebuggerStatement hook, the test's own microtasks don't get suspended
   * along with the debuggee's.
   */
  do_timeout(0, () => {
    /* eslint-disable */
    Cu.evalInSandbox(
      "var line0 = Error().lineNumber;\n" +
      "debugger;\n" + // line0 + 1
      "var a = 1;\n" + // line0 + 2
      "var b = 2;\n", // line0 + 3
      debuggee
    );
    /* eslint-enable */
  });
}
