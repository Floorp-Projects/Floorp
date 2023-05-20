/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check conditional breakpoint when condition evaluates to true.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    let hitBreakpoint = false;

    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet1.frame.where.actor);
    const location = { sourceUrl: source.url, line: 3 };
    threadFront.setBreakpoint(location, { condition: "a === 1" });

    // Continue until the breakpoint is hit.
    const packet2 = await resumeAndWaitForPause(threadFront);

    Assert.equal(hitBreakpoint, false);
    hitBreakpoint = true;

    // Check the return value.
    Assert.equal(packet2.why.type, "breakpoint");
    Assert.equal(packet2.frame.where.line, 3);

    // Remove the breakpoint.
    await threadFront.removeBreakpoint(location);

    await threadFront.resume();

    Assert.equal(hitBreakpoint, true);
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "debugger;\n" + // line 1
      "var a = 1;\n" + // line 2
      "var b = 2;\n", // line 3
    debuggee,
    "1.8",
    "test.js",
    1
  );
  /* eslint-enable */
}
