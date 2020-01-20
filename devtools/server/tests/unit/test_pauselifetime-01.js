/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that pause-lifetime grips go away correctly after a resume.
 */

var gDebuggee;
var gClient;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      gClient = client;
      test_pause_frame();
    },
    { waitForFinish: true }
  )
);

function test_pause_frame() {
  gThreadFront.once("paused", async function(packet) {
    const pauseActor = packet.actor;

    // Make a bogus request to the pause-lifetime actor.  Should get
    // unrecognized-packet-type (and not no-such-actor).
    try {
      await gClient.request({ to: pauseActor, type: "bogusRequest" });
      ok(false, "bogusRequest should throw");
    } catch (e) {
      ok(true, "bogusRequest thrown");
      Assert.equal(e.error, "unrecognizedPacketType");
    }

    gThreadFront.resume().then(async function() {
      // Now that we've resumed, should get no-such-actor for the
      // same request.
      try {
        await gClient.request({ to: pauseActor, type: "bogusRequest" });
        ok(false, "bogusRequest should throw");
      } catch (e) {
        ok(true, "bogusRequest thrown");
        Assert.equal(e.error, "noSuchActor");
      }
      threadFrontTestFinished();
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe() {
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
