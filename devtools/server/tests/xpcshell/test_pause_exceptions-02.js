/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true when the debugger isn't in a
 * paused state will not cause the debuggee to pause when an exception is thrown.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, commands }) => {
    await commands.threadConfigurationCommand.updateConfiguration({
      pauseOnExceptions: true,
      ignoreCaughtExceptions: false,
    });

    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    Assert.equal(packet.why.type, "exception");
    Assert.equal(packet.why.exception, 42);
    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable no-throw-literal */
  // prettier-ignore
  debuggee.eval("(" + function () {    // 1
    function stopMe() {                // 2
      throw 42;                        // 3
    }                                  // 4
    try {                              // 5
      stopMe();                        // 6
    } catch (e) {}                     // 7
  } + ")()");
}
