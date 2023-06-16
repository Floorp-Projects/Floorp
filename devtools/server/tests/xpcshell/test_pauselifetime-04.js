/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that requesting a pause actor for the same value multiple
 * times returns the same actor.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const args = packet.frame.arguments;
    const objActor1 = args[0].actor;

    const response = await threadFront.getFrames(0, 1);
    const frame = response.frames[0];
    Assert.equal(objActor1, frame.arguments[0].actor);

    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        function stopMe(obj) {
          debugger;
        }
        stopMe({ foo: "bar" });
      } +
      ")()"
  );
}
