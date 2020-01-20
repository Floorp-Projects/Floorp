/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that stepping over a function call does not pause inside the function.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");
    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    dumpn("Step Over to f()");
    const step1 = await stepOver(threadFront);
    equal(step1.why.type, "resumeLimit");
    equal(step1.frame.where.line, 6);
    equal(debuggee.a, undefined);
    equal(debuggee.b, undefined);

    dumpn("Step Over f()");
    const step2 = await stepOver(threadFront);
    equal(step2.frame.where.line, 7);
    equal(step2.why.type, "resumeLimit");
    equal(debuggee.a, 1);
    equal(debuggee.b, undefined);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   // 1
    function f() {                      // 2
      this.a = 1;                       // 3
    }                                   // 4
    debugger;                           // 5
    f();                                // 6
    let b = 2;                          // 7
    `,                                  // 8
    debuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
  /* eslint-disable */
}
