/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that pause-lifetime grips go away correctly after a resume.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const pauseActor = packet.actor;

    // Make a bogus request to the pause-lifetime actor. Should get
    // unrecognized-packet-type (and not no-such-actor).
    try {
      await client.request({ to: pauseActor, type: "bogusRequest" });
      ok(false, "bogusRequest should throw");
    } catch (e) {
      ok(true, "bogusRequest thrown");
      Assert.equal(e.error, "unrecognizedPacketType");
    }

    await threadFront.resume();

    // Now that we've resumed, should get no-such-actor for the
    // same request.
    try {
      await client.request({ to: pauseActor, type: "bogusRequest" });
      ok(false, "bogusRequest should throw");
    } catch (e) {
      ok(true, "bogusRequest thrown");
      Assert.equal(e.error, "noSuchActor");
    }
  })
);

function evaluateTestCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        function stopMe() {
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
