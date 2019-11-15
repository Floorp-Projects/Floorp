/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Verify that a frame actor is properly expired when the frame goes away.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_pause_frame();
    },
    { waitForFinish: true }
  )
);

function test_pause_frame() {
  gThreadFront.once("paused", function(packet1) {
    gThreadFront.once("paused", function(packet2) {
      const poppedFrames = packet2.poppedFrames;
      Assert.equal(typeof poppedFrames, typeof []);
      Assert.ok(poppedFrames.includes(packet1.frame.actorID));
      gThreadFront.resume().then(function() {
        threadFrontTestFinished();
      });
    });
    gThreadFront.resume();
  });

  gDebuggee.eval(
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
