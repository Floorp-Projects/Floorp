/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that requesting a pause actor for the same value multiple
 * times returns the same actor.
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
  gThreadFront.once("paused", function(packet) {
    const args = packet.frame.arguments;
    const objActor1 = args[0].actor;

    gThreadFront.getFrames(0, 1).then(function(response) {
      const frame = response.frames[0];
      Assert.equal(objActor1, frame.arguments[0].actor);
      gThreadFront.resume().then(function() {
        threadFrontTestFinished();
      });
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe(obj) {
          debugger;
        }
        stopMe({ foo: "bar" });
      } +
      ")()"
  );
}
