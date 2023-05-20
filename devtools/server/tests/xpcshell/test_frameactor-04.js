/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify the "frames" request on the thread.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const response = await threadFront.getFrames(0, 1000);
    for (let i = 0; i < response.frames.length; i++) {
      const expected = frameFixtures[i];
      const actual = response.frames[i];

      Assert.equal(
        expected.displayname,
        actual.displayname,
        "Frame displayname"
      );
      Assert.equal(expected.type, actual.type, "Frame displayname");
    }

    await threadFront.resume();
  })
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

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
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
