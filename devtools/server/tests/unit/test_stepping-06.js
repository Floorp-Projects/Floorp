/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that stepping out of a function returns the right return value.
 */

var gDebuggee;
var gClient;
var gThreadClient;
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
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(
      gClient, "test-stack",
      function(response, tabClient, threadClient) {
        gThreadClient = threadClient;
        // XXX: We have to do an executeSoon so that the error isn't caught and
        // reported by DebuggerClient.requester (because we are using the local
        // transport and share a stack) which causes the test to fail.
        Services.tm.dispatchToMainThread({
          run: test_simple_stepping
        });
      });
  });
}

async function test_simple_stepping() {
  await executeOnNextTickAndWaitForPause(evaluateTestCode, gClient);

  const step1 = await stepOut(gClient, gThreadClient);
  equal(step1.type, "paused");
  equal(step1.frame.where.line, 6);
  equal(step1.why.type, "resumeLimit");
  equal(step1.why.frameFinished.return, 10);

  gThreadClient.resume();
  const step2 = await waitForPause(gThreadClient);
  equal(step2.type, "paused");
  equal(step2.frame.where.line, 8);
  equal(step2.why.type, "debuggerStatement");

  gThreadClient.stepOut();
  const step3 = await waitForPause(gThreadClient);
  equal(step3.type, "paused");
  equal(step3.frame.where.line, 9);
  equal(step3.why.type, "resumeLimit");
  equal(step3.why.frameFinished.return.type, "undefined");

  gThreadClient.resume();
  const step4 = await waitForPause(gThreadClient);

  equal(step4.type, "paused");
  equal(step4.frame.where.line, 11);

  gThreadClient.stepOut();
  const step5 = await waitForPause(gThreadClient);
  equal(step5.type, "paused");
  equal(step5.frame.where.line, 12);
  equal(step5.why.type, "resumeLimit");
  equal(step5.why.frameFinished.throw, "ah");

  finishClient(gClient, gCallback);
}

function evaluateTestCode() {
  /* eslint-disable */
  Cu.evalInSandbox(
    `                                   //  1
    function f() {                      //  2
      debugger;                         //  3
      var a = 10;                       //  4
      return a;                         //  5
    }                                   //  6
    function g() {                      //  7
      debugger;                         //  8
    }                                   //  9
    function h() {                      // 10
      debugger;                         // 11
      throw 'ah';                       // 12
      return 2;                         // 13
    }                                   // 14
    f()                                 // 15
    g()                                 // 16
    try {                               // 17
      h();                              // 18
    } catch (ex) { };                   // 19
    `,                                  // 20
    gDebuggee,
    "1.8",
    "test_stepping-07-test-code.js",
    1
  );
  /* eslint-enable */
}
