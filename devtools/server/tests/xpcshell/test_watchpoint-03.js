/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";
/*
See Bug 1601311.
Tests that removing a watchpoint does not change the value of the property that had the watchpoint.
*/

add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    async function evaluateJS(input) {
      const { result } = await commands.scriptCommand.execute(input, {
        frameActor: packet.frame.actorID,
      });
      return result;
    }

    function evaluateTestCode(debuggee) {
      /* eslint-disable */
      Cu.evalInSandbox(
        `                                   // 1
                  function stopMe(obj) {              // 2
                    debugger;                         // 3
                    obj.a = 2;                        // 4
                    debugger;                         // 5
                  }                                   //

                  stopMe({a: 1})`,
        debuggee,
        "1.8",
        "test_watchpoint-03.js"
      );
      /* eslint-disable */
    }

    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    info("Test that we paused on the debugger statement.");
    Assert.equal(packet.frame.where.line, 3);

    info("Add set watchpoint.");
    const args = packet.frame.arguments;
    const obj = args[0];
    const objClient = threadFront.pauseGrip(obj);
    await objClient.addWatchpoint("a", "obj.a", "set");

    info("Test that we pause on set.");
    const packet2 = await resumeAndWaitForPause(threadFront);
    Assert.equal(packet2.frame.where.line, 4);

    const packet3 = await resumeAndWaitForPause(threadFront);

    info("Test that we pause on the second debugger statement.");
    Assert.equal(packet3.frame.where.line, 5);
    Assert.equal(packet3.why.type, "debuggerStatement");

    info("Remove watchpoint.");
    await objClient.removeWatchpoint("a");

    info("Test that the value has updated.");
    const result = await evaluateJS("obj.a");
    Assert.equal(result, 2);

    info("Finish test.");
    await resume(threadFront);
  })
);
