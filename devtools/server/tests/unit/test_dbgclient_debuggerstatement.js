/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gClient;
var gTabClient;
var gDebuggee;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(function ([type, traits]) {
    attachTestTab(gClient, "test-1", function (reply, tabClient) {
      gTabClient = tabClient;
      test_threadAttach(reply.threadActor);
    });
  });
  do_test_pending();
}

function test_threadAttach(threadActorID) {
  do_print("Trying to attach to thread " + threadActorID);
  gTabClient.attachThread({}, function (response, threadClient) {
    do_check_eq(threadClient.state, "paused");
    do_check_eq(threadClient.actor, threadActorID);
    threadClient.resume(function () {
      do_check_eq(threadClient.state, "attached");
      test_debugger_statement(threadClient);
    });
  });
}

function test_debugger_statement(threadClient) {
  threadClient.addListener("paused", function (event, packet) {
    do_check_eq(threadClient.state, "paused");
    // Reach around the protocol to check that the debuggee is in the state
    // we expect.
    do_check_true(gDebuggee.a);
    do_check_false(gDebuggee.b);

    let xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
    do_check_eq(xpcInspector.eventLoopNestLevel, 1);

    threadClient.resume(cleanup);
  });

  Cu.evalInSandbox("var a = true; var b = false; debugger; var b = true;", gDebuggee);

  // Now make sure that we've run the code after the debugger statement...
  do_check_true(gDebuggee.b);
}

function cleanup() {
  gClient.addListener("closed", function (event) {
    do_test_finished();
  });

  try {
    let xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
    do_check_eq(xpcInspector.eventLoopNestLevel, 0);
  } catch (e) {
    dump(e);
  }

  gClient.close();
}
