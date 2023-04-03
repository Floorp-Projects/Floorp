/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic step-out functionality.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");
    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const step1 = await stepOut(threadFront);
    equal(step1.frame.where.line, 8);
    equal(step1.why.type, "resumeLimit");

    equal(debuggee.a, 1);
    equal(debuggee.b, 2);
  })
);

function evaluateTestCode(debuggee) {
  // prettier-ignore
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
}
