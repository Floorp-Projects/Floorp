/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that execution doesn't pause twice while stepping, when encountering
 * either a breakpoint or a debugger statement.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet.frame.where.actor);
    await threadFront.setBreakpoint(
      { sourceUrl: source.url, line: 3, column: 6 },
      {}
    );

    info("Check that the stepping worked.");
    const packet1 = await stepIn(threadFront);
    Assert.equal(packet1.frame.where.line, 6);
    Assert.equal(packet1.why.type, "resumeLimit");

    info("Entered the foo function call frame.");
    const packet2 = await stepIn(threadFront);
    Assert.equal(packet2.frame.where.line, 3);
    Assert.equal(packet2.why.type, "resumeLimit");

    info("Check that the breakpoint wasn't the reason for this pause");
    const packet3 = await stepIn(threadFront);
    Assert.equal(packet3.frame.where.line, 4);
    Assert.equal(packet3.why.type, "resumeLimit");
    Assert.equal(packet3.why.frameFinished.return.type, "undefined");

    info("Check that the debugger statement wasn't the reason for this pause.");
    const packet4 = await stepIn(threadFront);
    Assert.equal(debuggee.a, 1);
    Assert.equal(debuggee.b, undefined);
    Assert.equal(packet4.frame.where.line, 7);
    Assert.equal(packet4.why.type, "resumeLimit");
    Assert.equal(packet4.poppedFrames.length, 1);

    info("Check that the debugger statement wasn't the reason for this pause.");
    const packet5 = await stepIn(threadFront);
    Assert.equal(packet5.frame.where.line, 8);
    Assert.equal(packet5.why.type, "resumeLimit");

    info("Remove the breakpoint and finish.");
    await stepIn(threadFront);
    threadFront.removeBreakpoint({ sourceUrl: source.url, line: 3 });

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `
  function foo() {
    this.a = 1; // <-- breakpoint set here
  }
  debugger;
  foo();
  debugger;
  var b = 2;
  `,
    debuggee,
    "1.8",
    "test_breakpoint-13.js",
    1
  );
  /* eslint-enable */
}
