/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check basic step-out functionality.
 */

var gDebuggee;
var gClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
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
  await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);

  const step1 = await stepOut(gClient, threadClient);
  equal(step1.type, "paused");
  equal(step1.frame.where.line, 8);
  equal(step1.why.type, "resumeLimit");

  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, 2);

  finishClient(gClient, gCallback);
}

function evaluateTestCode() {
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
    gDebuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
  /* eslint-disable */
}
