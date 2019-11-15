/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Verify that two pauses in a row will keep the same frame actor.
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
      Assert.equal(packet1.frame.actor, packet2.frame.actor);
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
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
