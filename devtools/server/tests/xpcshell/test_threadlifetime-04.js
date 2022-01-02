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
  gThreadFront.once("paused", function(packet) {
    const pauseGrip = packet.frame.arguments[0];

    gClient.request({ to: pauseGrip.actor, type: "threadGrip" }, function(
      response
    ) {
      // Successful promotion won't return an error.
      Assert.equal(response.error, undefined);

      const threadGrip1 = response.from;

      gClient.request({ to: pauseGrip.actor, type: "threadGrip" }, function(
        response
      ) {
        Assert.equal(threadGrip1, response.from);
        gThreadFront.resume().then(function() {
          threadFrontTestFinished();
        });
      });
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe(arg1) {
          debugger;
        }
        stopMe({ obj: true });
      } +
      ")()"
  );
}
