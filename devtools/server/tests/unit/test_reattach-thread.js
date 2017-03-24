/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that reattaching to a previously detached thread works.
 */

var gClient, gDebuggee, gThreadClient, gTabClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-reattach");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(() => {
    attachTestTab(gClient, "test-reattach", (reply, tabClient) => {
      gTabClient = tabClient;
      test_attach();
    });
  });
  do_test_pending();
}

function test_attach() {
  gTabClient.attachThread({}, (response, threadClient) => {
    do_check_eq(threadClient.state, "paused");
    gThreadClient = threadClient;
    threadClient.resume(test_detach);
  });
}

function test_detach() {
  gThreadClient.detach(() => {
    do_check_eq(gThreadClient.state, "detached");
    do_check_eq(gTabClient.thread, null);
    test_reattach();
  });
}

function test_reattach() {
  gTabClient.attachThread({}, (response, threadClient) => {
    do_check_neq(gThreadClient, threadClient);
    do_check_eq(threadClient.state, "paused");
    do_check_eq(gTabClient.thread, threadClient);
    threadClient.resume(cleanup);
  });
}

function cleanup() {
  gClient.close().then(do_test_finished);
}
