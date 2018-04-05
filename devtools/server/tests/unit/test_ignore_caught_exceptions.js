/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting ignoreCaughtExceptions will cause the debugger to ignore
 * caught exceptions, but not uncaught ones.
 */

var gDebuggee;
var gClient;

function run_test() {
  do_test_pending();
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
}

function run_test_with_server(server, callback) {
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-pausing", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect(test_pause_frame);
}

async function test_pause_frame() {
  const [,, threadClient] = await attachTestTabAndResume(gClient, "test-pausing");
  await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);

  evaluateTestCode();

  threadClient.pauseOnExceptions(true, true);
  await resume(threadClient);
  const paused = await waitForPause(gClient);
  Assert.equal(paused.why.type, "exception");
  equal(paused.frame.where.line, 6, "paused at throw");

  await resume(threadClient);
  finishClient(gClient);
}

function evaluateTestCode() {
  /* eslint-disable */
  try {
  Cu.evalInSandbox(`                    // 1
   debugger;                            // 2
   try {                                // 3           
     throw "foo";                       // 4
   } catch (e) {}                       // 5
   throw "bar";                         // 6  
  `,                                    // 7
    gDebuggee,
    "1.8",
    "test_pause_exceptions-03.js",
    1
  );
  } catch (e) {}
  /* eslint-disable */
}
