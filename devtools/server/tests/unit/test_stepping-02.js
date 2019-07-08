/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check basic step-in functionality.
 */

add_task(
  threadClientTest(async ({ threadClient, debuggee }) => {
    dumpn("Evaluating test code and waiting for first debugger statement");
    const dbgStmt = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadClient
    );
    equal(
      dbgStmt.frame.where.line,
      2,
      "Should be at debugger statement on line 2"
    );
    equal(debuggee.a, undefined);
    equal(debuggee.b, undefined);

    const step1 = await stepIn(threadClient);
    equal(step1.why.type, "resumeLimit");
    equal(step1.frame.where.line, 3);
    equal(debuggee.a, undefined);
    equal(debuggee.b, undefined);

    const step3 = await stepIn(threadClient);
    equal(step3.why.type, "resumeLimit");
    equal(step3.frame.where.line, 4);
    equal(debuggee.a, 1);
    equal(debuggee.b, undefined);

    const step4 = await stepIn(threadClient);
    equal(step4.why.type, "resumeLimit");
    equal(step4.frame.where.line, 4);
    equal(debuggee.a, 1);
    equal(debuggee.b, 2);
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
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
  /* eslint-disable */
}
