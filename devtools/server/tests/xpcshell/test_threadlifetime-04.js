/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that requesting a thread-lifetime actor twice for the same
 * value returns the same actor.
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
  gThreadFront.once("paused", async function (packet) {
    const pauseGrip = packet.frame.arguments[0];

    const response = await gClient.request({
      to: pauseGrip.actor,
      type: "threadGrip",
    });
    const threadGrip1 = response.from;

    const response2 = await gClient.request({
      to: pauseGrip.actor,
      type: "threadGrip",
    });
    Assert.equal(threadGrip1, response2.from);
    await gThreadFront.resume();

    threadFrontTestFinished();
  });

  gDebuggee.eval(
    "(" +
      function () {
        // These arguments are tested.
        // eslint-disable-next-line no-unused-vars
        function stopMe(arg1) {
          debugger;
        }
        stopMe({ obj: true });
      } +
      ")()"
  );
}
