/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's arguments property.
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
    Assert.equal(args.length, 6);
    Assert.equal(args[0], 42);
    Assert.equal(args[1], true);
    Assert.equal(args[2], "nasu");
    Assert.equal(args[3].type, "null");
    Assert.equal(args[4].type, "undefined");
    Assert.equal(args[5].type, "object");
    Assert.equal(args[5].class, "Object");
    Assert.ok(!!args[5].actor);

    gThreadFront.resume().then(function() {
      threadFrontTestFinished();
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe(number, bool, string, null_, undef, object) {
          debugger;
        }
        stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
      } +
      ")()"
  );
}
