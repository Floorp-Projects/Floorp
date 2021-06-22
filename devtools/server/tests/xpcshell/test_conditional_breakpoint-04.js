/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Confirm that we ignore breakpoint condition exceptions
 * unless pause-on-exceptions is set to true.
 *
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await threadFront.setBreakpoint(
      { sourceUrl: "conditional_breakpoint-04.js", line: 3 },
      { condition: "throw new Error()" }
    );

    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    Assert.equal(packet.frame.where.line, 1);
    Assert.equal(packet.why.type, "debuggerStatement");

    threadFront.resume();
    const pausedPacket = await waitForEvent(threadFront, "paused");
    Assert.equal(pausedPacket.frame.where.line, 4);
    Assert.equal(pausedPacket.why.type, "debuggerStatement");

    // Remove the breakpoint.
    await threadFront.removeBreakpoint({
      sourceUrl: "conditional_breakpoint-04.js",
      line: 3,
    });
    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `debugger;
    var a = 1;
    var b = 2;
    debugger;`,
    debuggee,
    "1.8",
    "conditional_breakpoint-04.js",
    1
  );
  /* eslint-enable */
}
