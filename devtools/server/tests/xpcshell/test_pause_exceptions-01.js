/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true will cause the debuggee to pause
 * when an exception is thrown.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, commands }) => {
    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    await commands.threadConfigurationCommand.updateConfiguration({
      pauseOnExceptions: true,
      ignoreCaughtExceptions: false,
    });
    const packet = await resumeAndWaitForPause(threadFront);
    Assert.equal(packet.why.type, "exception");
    Assert.equal(packet.why.exception, 42);

    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable no-throw-literal */
  // prettier-ignore
  debuggee.eval("(" + function () {
    function stopMe() {
      debugger;
      throw 42;
    }
    try {
      stopMe();
    } catch (e) {}
  } + ")()");
  /* eslint-enable no-throw-literal */
}
