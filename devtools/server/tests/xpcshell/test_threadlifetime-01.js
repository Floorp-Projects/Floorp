/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that thread-lifetime grips last past a resume.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const pauseGrip = packet.frame.arguments[0];

    // Create a thread-lifetime actor for this object.
    const response = await client.request({
      to: pauseGrip.actor,
      type: "threadGrip",
    });
    // Successful promotion won't return an error.
    Assert.equal(response.error, undefined);

    const packet2 = await resumeAndWaitForPause(threadFront);

    // Verify that the promoted actor is returned again.
    Assert.equal(pauseGrip.actor, packet2.frame.arguments[0].actor);
    // Now that we've resumed, should get unrecognizePacketType for the
    // promoted grip.
    try {
      await client.request({ to: pauseGrip.actor, type: "bogusRequest" });
      ok(false, "bogusRequest should throw");
    } catch (e) {
      Assert.equal(e.error, "unrecognizedPacketType");
      ok(true, "bogusRequest thrown");
    }
    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        // These arguments are tested.
        // eslint-disable-next-line no-unused-vars
        function stopMe(arg1) {
          debugger;
          debugger;
        }
        stopMe({ obj: true });
      } +
      ")()"
  );
}
