/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that two pauses in a row will keep the same frame actor.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const packet2 = await resumeAndWaitForPause(threadFront);

    Assert.equal(packet1.frame.actor, packet2.frame.actor);
    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        function stopMe() {
          debugger;
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
