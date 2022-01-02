/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that a frame actor is properly expired when the frame goes away.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );
    const frameActorID = packet.frame.actorID;
    {
      const { frames } = await threadFront.getFrames(0, null);
      ok(
        frames.some(f => f.actorID === frameActorID),
        "The paused frame is returned by getFrames"
      );

      Assert.equal(frames.length, 3, "Thread front has 3 frames");
    }

    threadFront.resume();
    await waitForPause(threadFront);
    await checkFramesLength(threadFront, 2);
    {
      const { frames } = await threadFront.getFrames(0, null);
      ok(
        !frames.some(f => f.actorID === frameActorID),
        "The paused frame is no longer returned by getFrames"
      );

      Assert.equal(frames.length, 2, "Thread front has 2 frames");
    }
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
