/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we get a frame actor along with a debugger statement.
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
    Assert.ok(!!packet.frame);
    Assert.ok(!!packet.frame.actor);
    Assert.equal(packet.frame.displayName, "stopMe");
    gThreadFront.resume().then(function() {
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
