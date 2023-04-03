/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic step-over functionality with pause points
 * for the first statement and end of the last statement.
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
    equal(debuggee.a, undefined);
    equal(debuggee.b, undefined);

    const source = await getSource(
      threadFront,
      "test_stepping-01-test-code.js"
    );

    // Add pause points for the first and end of the last statement.
    // Note: we intentionally ignore the second statement.
    source.setPausePoints([
      {
        location: { line: 3, column: 8 },
        types: { breakpoint: true, stepOver: true },
      },
      {
        location: { line: 4, column: 14 },
        types: { breakpoint: true, stepOver: true },
      },
    ]);

    dumpn("Step Over to line 3");
    const step1 = await stepOver(threadFront);
    equal(step1.why.type, "resumeLimit");
    equal(step1.frame.where.line, 3);
    equal(step1.frame.where.column, 12);

    equal(debuggee.a, undefined);
    equal(debuggee.b, undefined);

    dumpn("Step Over to line 4");
    const step2 = await stepOver(threadFront);
    equal(step2.why.type, "resumeLimit");
    equal(step2.frame.where.line, 4);
    equal(step2.frame.where.column, 12);

    equal(debuggee.a, 1);
    equal(debuggee.b, undefined);

    dumpn("Step Over to the end of line 4");
    const step3 = await stepOver(threadFront);
    equal(step3.why.type, "resumeLimit");
    equal(step3.frame.where.line, 4);
    equal(step3.frame.where.column, 14);
    equal(debuggee.a, 1);
    equal(debuggee.b, 2);
  })
);

function evaluateTestCode(debuggee) {
  // prettier-ignore
  Cu.evalInSandbox(
    `                                   // 1
    debugger;                           // 2
    var a = 1;                          // 3
    var b = 2;`,                        // 4
    debuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
}
