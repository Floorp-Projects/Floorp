/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the debugger automatically ignores NS_ERROR_NO_INTERFACE
 * exceptions, but not normal ones.
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

  await threadClient.pauseOnExceptions(true, false);
  await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);

  await resume(threadClient);
  const paused = await waitForPause(gClient);
  Assert.equal(paused.why.type, "exception");
  equal(paused.frame.where.line, 12, "paused at throw");

  await resume(threadClient);
  finishClient(gClient);
}

function evaluateTestCode() {
  /* eslint-disable */
  Cu.evalInSandbox(`                    // 1
    function QueryInterface() {         // 2
      throw Cr.NS_ERROR_NO_INTERFACE;   // 3
    }                                   // 4
    function stopMe() {                 // 5
      throw 42;                         // 6
    }                                   // 7
    try {                               // 8
      QueryInterface();                 // 9
    } catch (e) {}                      // 10
    try {                               // 11
      stopMe();                         // 12
    } catch (e) {}`,                    // 13
    gDebuggee,
    "1.8",
    "test_ignore_no_interface_exceptions.js",
    1
  );
  /* eslint-disable */
}
