/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we get a frame actor along with a debugger statement.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    Assert.ok(!!packet.frame);
    Assert.ok(!!packet.frame.getActorByID);
    Assert.equal(packet.frame.displayName, "stopMe");
    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    "(" +
      function () {
        function stopMe() {
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
