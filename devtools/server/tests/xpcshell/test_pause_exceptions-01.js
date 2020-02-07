/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting pauseOnExceptions to true will cause the debuggee to pause
 * when an exception is thrown.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    threadFront.pauseOnExceptions(true, false);
    threadFront.resume();

    const packet = await waitForPause(threadFront);
    Assert.equal(packet.why.type, "exception");
    Assert.equal(packet.why.exception, 42);

    threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  debuggee.eval("(" + function () {
    function stopMe() {
      debugger;
      throw 42;
    }
    try {
      stopMe();
    } catch (e) {}
  } + ")()");
  /* eslint-enable */
}
