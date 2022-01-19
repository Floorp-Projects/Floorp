/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we never pause while being already paused.
 * i.e. we don't support more than one nested event loops.
 */

add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    await threadFront.setBreakpoint({ sourceUrl: "nesting-04.js", line: 2 });

    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    Assert.equal(packet.frame.where.line, 5);
    Assert.equal(packet.why.type, "debuggerStatement");

    info("Test calling interrupt");
    const onPaused = waitForPause(threadFront);
    await threadFront.interrupt();
    // interrupt() doesn't return anything, but bailout while emitting a paused packet
    // But we don't pause again, the reason prove it so
    const paused = await onPaused;
    equal(paused.why.type, "alreadyPaused");

    info("Test by evaluating code via the console");
    const { result } = await commands.scriptCommand.execute(
      "debugger; functionWithDebuggerStatement()",
      {
        frameActor: packet.frame.actorID,
      }
    );
    // The fact that it returned immediately means that we did not pause
    equal(result, 42);

    info("Test by calling code from chrome context");
    // This should be equivalent to any actor somehow triggering some page's JS
    const rv = debuggee.functionWithDebuggerStatement();
    // The fact that it returned immediately means that we did not pause
    equal(rv, 42);

    info("Test by stepping over a function that breaks");
    // This will only step over the debugger; statement we just break on
    const step1 = await stepOver(threadFront);
    equal(step1.why.type, "resumeLimit");
    equal(step1.frame.where.line, 6);

    // stepOver will actually resume and re-pause on the breakpoint
    const step2 = await stepOver(threadFront);
    equal(step2.why.type, "breakpoint");
    equal(step2.frame.where.line, 2);

    // Sanity check to ensure that the functionWithDebuggerStatement really pauses
    info("Resume and pause on the breakpoint");
    const pausedPacket = await resumeAndWaitForPause(threadFront);
    Assert.equal(pausedPacket.frame.where.line, 2);
    // The breakpoint takes over the debugger statement
    Assert.equal(pausedPacket.why.type, "breakpoint");

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `function functionWithDebuggerStatement() {
      debugger;
      return 42;
    }
    debugger;
    functionWithDebuggerStatement();
    var a = 1;
    functionWithDebuggerStatement();`,
    debuggee,
    "1.8",
    "nesting-04.js",
    1
  );
  /* eslint-enable */
}
