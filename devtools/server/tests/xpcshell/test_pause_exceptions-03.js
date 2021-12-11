/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true will cause the debuggee to pause
 * when an exception is thrown.
 */

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, commands }) => {
      await executeOnNextTickAndWaitForPause(
        () => evaluateTestCode(debuggee),
        threadFront
      );

      await commands.threadConfigurationCommand.updateConfiguration({
        pauseOnExceptions: true,
        ignoreCaughtExceptions: false,
      });
      await resume(threadFront);
      const paused = await waitForPause(threadFront);
      Assert.equal(paused.why.type, "exception");
      equal(paused.frame.where.line, 4, "paused at throw");

      await resume(threadFront);
    },
    {
      // Bug 1508289, exception tests fails in worker scope
      doNotRunWorker: true,
    }
  )
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   // 1
    function stopMe() {                 // 2
      debugger;                         // 3
      throw 42;                         // 4
    }                                   // 5
    try {                               // 6
      stopMe();                         // 7
    } catch (e) {}`,                    // 8
    debuggee,
    "1.8",
    "test_pause_exceptions-03.js",
    1
  );
  /* eslint-disable */
}
