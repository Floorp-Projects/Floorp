/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * If pauseOnExceptions is checked, when condition throws,
 * make sure conditional breakpoint pauses.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, commands }) => {
    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet1.frame.where.actor);

    await commands.threadConfigurationCommand.updateConfiguration({
      pauseOnExceptions: true,
      ignoreCaughtExceptions: false,
    });
    const location = { sourceUrl: source.url, line: 3 };
    threadFront.setBreakpoint(location, { condition: "throw new Error()" });

    // Continue until the breakpoint is hit.
    threadFront.resume();

    const packet2 = await waitForPause(threadFront);

    // Check the return value.
    Assert.equal(packet2.why.type, "exception");
    Assert.equal(packet2.frame.where.line, 1);

    // Step over twice.
    await stepOver(threadFront);
    const packet3 = await stepOver(threadFront);

    // Check the return value.
    Assert.equal(packet3.why.type, "breakpointConditionThrown");
    Assert.equal(packet3.frame.where.line, 3);

    // Remove the breakpoint.
    await threadFront.removeBreakpoint(location);
    threadFront.resume();
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
