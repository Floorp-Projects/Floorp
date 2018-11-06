/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out stops at the end of the parent if it fails to stop
 * anywhere else. Bug 1504358.
 */

var gDebuggee;
var gClient;
var gCallback;

function run_test() {
  do_test_pending();
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stepping", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect(testStepOutWithBreakpoint);
}

async function testStepOutWithBreakpoint() {
  const [attachResponse,, threadClient] = await attachTestTabAndResume(gClient,
                                                                      "test-stepping");
  ok(!attachResponse.error, "Should not get an error attaching");

  dumpn("Evaluating test code and waiting for first debugger statement");
  const dbgStmt = await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);
  equal(dbgStmt.frame.where.line, 2, "Should be at debugger statement on line 2");

  dumpn("Step out of inner and into outer");
  const step2 = await stepOut(gClient, threadClient);
  // The bug was that we'd step right past the end of the function and never pause.
  equal(step2.frame.where.line, 2);
  equal(step2.why.frameFinished.return, 42);

  finishClient(gClient, gCallback);
}

function evaluateTestCode() {
  // By placing the inner and outer on the same line, this triggers the server's
  // logic to skip steps for these functions, meaning that onPop is the only
  // thing that will cause it to pop.
  Cu.evalInSandbox(
    `
    function outer(){ inner(); return 42; } function inner(){ debugger; }
    outer();
    `,
    gDebuggee,
    "1.8",
    "test_stepping-09-test-code.js",
    1
  );
}
