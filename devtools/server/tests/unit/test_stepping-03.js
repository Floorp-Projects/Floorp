/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check basic step-out functionality.
 */

add_task(
  threadClientTest(async ({ threadClient, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");
    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadClient
    );

    const step1 = await stepOut(threadClient);
    equal(step1.frame.where.line, 8);
    equal(step1.why.type, "resumeLimit");

    equal(debuggee.a, 1);
    equal(debuggee.b, 2);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   // 1
    function f() {                      // 2
      debugger;                         // 3
      this.a = 1;                       // 4
      this.b = 2;                       // 5
    }                                   // 6
    f();                                // 7
    `,                                  // 8
    debuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
  /* eslint-disable */
}
