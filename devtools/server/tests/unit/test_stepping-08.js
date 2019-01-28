/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out doesn't double stop on a breakpoint.  Bug 970469.
 */

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  dumpn("Evaluating test code and waiting for first debugger statement");
  const dbgStmt = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee), client);
  equal(dbgStmt.frame.where.line, 3, "Should be at debugger statement on line 3");

  dumpn("Setting breakpoint in innerFunction");
  const source = await getSourceById(
    threadClient,
    dbgStmt.frame.where.actor
  );
  await source.setBreakpoint({ line: 7 });

  dumpn("Step in to innerFunction");
  const step1 = await stepIn(client, threadClient);
  equal(step1.frame.where.line, 7);

  dumpn("Step out of innerFunction");
  const step2 = await stepOut(client, threadClient);
  // The bug was that we'd stop again at the breakpoint on line 7.
  equal(step2.frame.where.line, 4);
}));

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   //  1
    function outerFunction() {          //  2
      debugger; innerFunction();        //  3
    }                                   //  4
                                        //  5
    function innerFunction() {          //  6
      var x = 0;                        //  7
      var y = 72;                       //  8
      return x+y;                       //  9
    }                                   // 10
    outerFunction();                    // 11
    `,                                  // 12
    debuggee,
    "1.8",
    "test_stepping-08-test-code.js",
    1
  );
  /* eslint-enable */
}
