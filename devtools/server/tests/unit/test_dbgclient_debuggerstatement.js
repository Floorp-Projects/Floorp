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

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(function([type, traits]) {
    attachTestTab(gClient, "test-1", function(reply, tabClient) {
      gTabClient = tabClient;
      test_threadAttach(reply.threadActor);
    });
  });
  do_test_pending();
}

function test_threadAttach(threadActorID) {
  info("Trying to attach to thread " + threadActorID);
  gTabClient.attachThread({}).then(function([response, threadClient]) {
    Assert.equal(threadClient.state, "paused");
    Assert.equal(threadClient.actor, threadActorID);
    threadClient.resume(function() {
      Assert.equal(threadClient.state, "attached");
      test_debugger_statement(threadClient);
    });
  });
}

function test_debugger_statement(threadClient) {
  threadClient.addListener("paused", function(event, packet) {
    Assert.equal(threadClient.state, "paused");
    // Reach around the protocol to check that the debuggee is in the state
    // we expect.
    Assert.ok(gDebuggee.a);
    Assert.ok(!gDebuggee.b);

    const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
    Assert.equal(xpcInspector.eventLoopNestLevel, 1);

    threadClient.resume(cleanup);
  });

  Cu.evalInSandbox("var a = true; var b = false; debugger; var b = true;", gDebuggee);

  // Now make sure that we've run the code after the debugger statement...
  Assert.ok(gDebuggee.b);
}

function cleanup() {
  gClient.addListener("closed", function(event) {
    do_test_finished();
  });

  try {
    const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
    Assert.equal(xpcInspector.eventLoopNestLevel, 0);
  } catch (e) {
    dump(e);
  }

  gClient.close();
}
