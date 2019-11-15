/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify the "frames" request on the thread.
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

var frameFixtures = [
  // Function calls...
  { type: "call", displayName: "depth3" },
  { type: "call", displayName: "depth2" },
  { type: "call", displayName: "depth1" },

  // Anonymous function call in our eval...
  { type: "call", displayName: undefined },

  // The eval itself.
  { type: "eval", displayName: "(eval)" },
];

async function test_frame_packet() {
  const response = await gThreadFront.getFrames(0, 1000);
  for (let i = 0; i < response.frames.length; i++) {
    const expected = frameFixtures[i];
    const actual = response.frames[i];

    Assert.equal(expected.displayname, actual.displayname, "Frame displayname");
    Assert.equal(expected.type, actual.type, "Frame displayname");
  }

  await gThreadFront.resume();
  threadFrontTestFinished();
}

function test_pause_frame() {
  gThreadFront.once("paused", function(packet) {
    test_frame_packet();
  });

  gDebuggee.eval(
    "(" +
      function() {
        function depth3() {
          debugger;
        }
        function depth2() {
          depth3();
        }
        function depth1() {
          depth2();
        }
        depth1();
      } +
      ")()"
  );
}
