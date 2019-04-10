/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the debugger automatically ignores NS_ERROR_NO_INTERFACE
 * exceptions, but not normal ones.
 */

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  await threadClient.pauseOnExceptions(true, false);
  const paused =
    await executeOnNextTickAndWaitForPause(() => evaluateTestCode(debuggee), client);
  equal(paused.frame.where.line, 6, "paused at throw");

  await resume(threadClient);
}, {
  // Bug 1508289, exception tests fails in worker scope
  doNotRunWorker: true,
}));

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(`                    // 1
    function QueryInterface() {         // 2
      throw Cr.NS_ERROR_NO_INTERFACE;   // 3
    }                                   // 4
    function stopMe() {                 // 5
      throw 42;                         // 6
    }                                   // 7
    try {                               // 8
      QueryInterface();                 // 9
    } catch (e) {}                      // 10
    try {                               // 11
      stopMe();                         // 12
    } catch (e) {}`,                    // 13
    debuggee,
    "1.8",
    "test_ignore_no_interface_exceptions.js",
    1
  );
  /* eslint-disable */
}
