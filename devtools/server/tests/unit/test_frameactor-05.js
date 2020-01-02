/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Verify that frame actors retrieved with the frames request
 * are included in the pause packet's popped-frames property.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const frameResponse = await threadFront.getFrames(0, null);
    Assert.equal(frameResponse.frames.length, 5);
    // Now wait for the next pause, after which the three
    // youngest actors should be popped..
    const expectPopped = frameResponse.frames
      .slice(0, 3)
      .map(frame => frame.actorID);
    expectPopped.sort();

    threadFront.resume();
    const pausePacket = await waitForPause(threadFront);

    const popped = pausePacket.poppedFrames.sort();
    Assert.equal(popped.length, 3);
    for (let i = 0; i < 3; i++) {
      Assert.equal(expectPopped[i], popped[i]);
    }

    threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function() {
        function depth3() {
          debugger;
        }
        function depth2() {
          depth3();
        }
        function depth1() {
          depth2();
        }
        depth1();
        debugger;
      } +
      ")()"
  );
}
