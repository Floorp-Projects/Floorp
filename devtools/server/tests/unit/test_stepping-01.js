/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check basic step-over functionality.
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
  gClient.connect(test_simple_stepping);
}

async function test_simple_stepping() {
  const [attachResponse,, threadClient] = await attachTestTabAndResume(gClient,
                                                                       "test-stepping");
  ok(!attachResponse.error, "Should not get an error attaching");

  dumpn("Evaluating test code and waiting for first debugger statement");
  const dbgStmt = await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);
  equal(dbgStmt.frame.where.line, 2, "Should be at debugger statement on line 2");
  equal(gDebuggee.a, undefined);
  equal(gDebuggee.b, undefined);

  dumpn("Step Over to line 3");
  const step1 = await stepOver(gClient, threadClient);
  equal(step1.type, "paused");
  equal(step1.why.type, "resumeLimit");
  equal(step1.frame.where.line, 3);
  equal(gDebuggee.a, undefined);
  equal(gDebuggee.b, undefined);

  dumpn("Step Over to line 4");
  const step3 = await stepOver(gClient, threadClient);
  equal(step3.type, "paused");
  equal(step3.why.type, "resumeLimit");
  equal(step3.frame.where.line, 4);
  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, undefined);

  dumpn("Step Over to line 4 to leave the frame");
  const step4 = await stepOver(gClient, threadClient);
  equal(step4.type, "paused");
  equal(step4.why.type, "resumeLimit");
  equal(step4.frame.where.line, 4);
  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, 2);

  finishClient(gClient, gCallback);
}

function evaluateTestCode() {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   // 1
    debugger;                           // 2
    var a = 1;                          // 3
    var b = 2;`,                        // 4
    gDebuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
  /* eslint-disable */
}