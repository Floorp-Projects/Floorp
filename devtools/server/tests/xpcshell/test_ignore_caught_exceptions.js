/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting ignoreCaughtExceptions will cause the debugger to ignore
 * caught exceptions, but not uncaught ones.
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
        ignoreCaughtExceptions: true,
      });
      await resume(threadFront);
      const paused = await waitForPause(threadFront);
      Assert.equal(paused.why.type, "exception");
      equal(paused.frame.where.line, 6, "paused at throw");

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
  try {
  Cu.evalInSandbox(`                    // 1
   debugger;                            // 2
   try {                                // 3
     throw "foo";                       // 4
   } catch (e) {}                       // 5
   throw "bar";                         // 6
  `,                                    // 7
    debuggee,
    "1.8",
    "test_pause_exceptions-03.js",
    1
  );
  } catch (e) {}
  /* eslint-disable */
}
