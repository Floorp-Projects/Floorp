/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check conditional breakpoint when condition evaluates to false.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet1.frame.where.actor);
    const location1 = { sourceUrl: source.url, line: 3 };
    threadFront.setBreakpoint(location1, { condition: "a === 2" });

    const location2 = { sourceUrl: source.url, line: 4 };
    threadFront.setBreakpoint(location2, { condition: "a === 1" });

    // Continue until the breakpoint is hit.
    threadFront.resume();
    const packet2 = await waitForPause(threadFront);

    // Check the return value.
    Assert.equal(packet2.why.type, "breakpoint");
    Assert.equal(packet2.frame.where.line, 4);

    // Remove the breakpoint.
    await threadFront.removeBreakpoint(location2);

    threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "debugger;\n" + // line 1
    "var a = 1;\n" + // line 2
    "var b = 2;\n" + // line 3
    "b++;" + // line 4
      "debugger;", // line 5
    debuggee,
    "1.8",
    "test.js",
    1
  );
  /* eslint-enable */
}
