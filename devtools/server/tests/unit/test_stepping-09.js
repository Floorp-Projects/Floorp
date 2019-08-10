/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out stops at the end of the parent if it fails to stop
 * anywhere else. Bug 1504358.
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
      2,
      "Should be at debugger statement on line 2"
    );

    dumpn("Step out of inner and into outer");
    const step2 = await stepOut(threadFront);
    // The bug was that we'd step right past the end of the function and never pause.
    equal(step2.frame.where.line, 2);
    equal(step2.frame.where.column, 31);
    deepEqual(step2.why.frameFinished.return, { type: "undefined" });
  })
);

function evaluateTestCode(debuggee) {
  // By placing the inner and outer on the same line, this triggers the server's
  // logic to skip steps for these functions, meaning that onPop is the only
  // thing that will cause it to pop.
  Cu.evalInSandbox(
    `
    function outer(){ inner(); return 42; } function inner(){ debugger; }
    outer();
    `,
    debuggee,
    "1.8",
    "test_stepping-09-test-code.js",
    1
  );
}
