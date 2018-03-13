/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that stepping in the last statement of the last frame doesn't
 * cause an unexpected pause, when another JS frame is pushed on the stack
 * (bug 785689).
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

  const step1 = await stepIn(gClient, threadClient);
  equal(step1.type, "paused");
  equal(step1.frame.where.line, 3);
  equal(step1.why.type, "resumeLimit");
  equal(gDebuggee.a, undefined);
  equal(gDebuggee.b, undefined);

  const step2 = await stepIn(gClient, threadClient);
  equal(step2.type, "paused");
  equal(step2.frame.where.line, 4);
  equal(step2.why.type, "resumeLimit");
  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, undefined);

  const step3 = await stepIn(gClient, threadClient);
  equal(step3.type, "paused");
  equal(step3.frame.where.line, 4);
  equal(step3.why.type, "resumeLimit");
  equal(gDebuggee.a, 1);
  equal(gDebuggee.b, 2);

  threadClient.stepIn(() => {
    threadClient.addOneTimeListener("paused", (event, packet) => {
      equal(packet.type, "paused");
      // Before fixing bug 785689, the type was resumeLimit.
      equal(packet.why.type, "debuggerStatement");
      finishClient(gClient, gCallback);
    });
    gDebuggee.eval("debugger;");
  });
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
    "test_stepping-05-test-code.js",
    1
  );
  /* eslint-disable */
}
