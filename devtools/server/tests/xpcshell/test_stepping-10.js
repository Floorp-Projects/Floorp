/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out stops at the parent and the parent's parent.
 * This checks for the failure found in bug 1530549.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");
    const dbgStmt = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );
    equal(
      dbgStmt.frame.where.line,
      3,
      "Should be at debugger statement on line 3"
    );

    dumpn("Step out of inner and into var statement IIFE");
    const step2 = await stepOut(threadFront);
    equal(step2.frame.where.line, 4);
    deepEqual(step2.why.frameFinished.return, { type: "undefined" });

    dumpn("Step out of vars and into script body");
    const step3 = await stepOut(threadFront);
    equal(step3.frame.where.line, 9);
    deepEqual(step3.why.frameFinished.return, { type: "undefined" });
  })
);

function evaluateTestCode(debuggee) {
  Cu.evalInSandbox(
    `
      (function() {
        (function(){debugger;})();
        var a = 1;
        a = 2;
        a = 3;
        a = 4;
      })();
    `,
    debuggee,
    "1.8",
    "test_stepping-10-test-code.js",
    1
  );
}
