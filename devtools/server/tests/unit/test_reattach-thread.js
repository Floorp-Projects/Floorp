/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that reattaching to a previously detached thread works.
 */

var gClient, gDebuggee, gThreadClient, gTargetFront;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-reattach");
  DebuggerServer.addTestGlobal(gDebuggee);

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(() => {
    attachTestTab(gClient, "test-reattach", (reply, targetFront) => {
      gTargetFront = targetFront;
      test_attach();
    });
  });
  do_test_pending();
}

function test_attach() {
  gTargetFront.attachThread({}).then(([response, threadClient]) => {
    Assert.equal(threadClient.state, "paused");
    gThreadClient = threadClient;
    threadClient.resume(test_detach);
  });
}

function test_detach() {
  gThreadClient.detach(() => {
    Assert.equal(gThreadClient.state, "detached");
    Assert.equal(gTargetFront.thread, null);
    test_reattach();
  });
}

function test_reattach() {
  gTargetFront.attachThread({}).then(([response, threadClient]) => {
    Assert.notEqual(gThreadClient, threadClient);
    Assert.equal(threadClient.state, "paused");
    Assert.equal(gTargetFront.thread, threadClient);
    threadClient.resume(cleanup);
  });
}

function cleanup() {
  gClient.close().then(do_test_finished);
}
