/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that thread-lifetime grips last past a resume.
 */

var gDebuggee;
var gClient;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      gClient = client;
      gDebuggee = debuggee;
      test_thread_lifetime();
    },
    { waitForFinish: true }
  )
);

function test_thread_lifetime() {
  gThreadFront.once("paused", async function(packet) {
    const pauseGrip = packet.frame.arguments[0];

    // Create a thread-lifetime actor for this object.
    const response = await gClient.request({
      to: pauseGrip.actor,
      type: "threadGrip",
    });
    // Successful promotion won't return an error.
    Assert.equal(response.error, undefined);
    gThreadFront.once("paused", async function(packet) {
      // Verify that the promoted actor is returned again.
      Assert.equal(pauseGrip.actor, packet.frame.arguments[0].actor);
      // Now that we've resumed, release the thread-lifetime grip.
      const objFront = new ObjectFront(
        gThreadFront.conn,
        gThreadFront.targetFront,
        gThreadFront,
        pauseGrip
      );
      await objFront.release();
      const objFront2 = new ObjectFront(
        gThreadFront.conn,
        gThreadFront.targetFront,
        gThreadFront,
        pauseGrip
      );

      try {
        await objFront2
          .request({ to: pauseGrip.actor, type: "bogusRequest" })
          .catch(function(error) {
            Assert.ok(!!error.message.match(/noSuchActor/));
            gThreadFront.resume().then(function() {
              threadFrontTestFinished();
            });
            throw new Error();
          });
        ok(false, "bogusRequest should throw");
      } catch (e) {
        ok(true, "bogusRequest thrown");
      }
    });
    gThreadFront.resume();
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe(arg1) {
          debugger;
          debugger;
        }
        stopMe({ obj: true });
      } +
      ")()"
  );
}
