/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that stepping over a function call does not pause inside the function.
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
  const [attachResponse,, threadClient] = await attachTestTabAndResume(
    gClient,
    "test-stepping"
  );

  ok(!attachResponse.error, "Should not get an error attaching");

  dumpn("Evaluating test code and waiting for first debugger statement");
  await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);

  dumpn("Step Over to f()");
  const step1 = await stepOver(gClient, threadClient);
  equal(step1.type, "paused");
  equal(step1.why.type, "resumeLimit");
  equal(step1.frame.where.line, 6);
  equal(gDebuggee.a, undefined);
  equal(gDebuggee.b, undefined);

  dumpn("Step Over f()");
  const step2 = await stepOver(gClient, threadClient);
  equal(step2.type, "paused");
  equal(step2.frame.where.line, 7);
  equal(step2.why.type, "resumeLimit");
  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, undefined);

  finishClient(gClient, gCallback);
}

function evaluateTestCode() {
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
    gDebuggee,
    "1.8",
    "test_stepping-01-test-code.js",
    1
  );
  /* eslint-disable */
}
