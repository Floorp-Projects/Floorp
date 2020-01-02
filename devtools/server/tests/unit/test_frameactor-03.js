/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Verify that a frame actor is properly expired when the frame goes away.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet1 = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    threadFront.resume();
    const packet2 = await waitForPause(threadFront);

    const poppedFrames = packet2.poppedFrames;
    Assert.equal(typeof poppedFrames, typeof []);
    Assert.ok(poppedFrames.includes(packet1.frame.actorID));

    threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function() {
        function stopMe() {
          debugger;
        }
        stopMe();
        debugger;
      } +
      ")()"
  );
}
