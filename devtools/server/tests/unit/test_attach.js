/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gClient;
var gDebuggee;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(function([type, traits]) {
    attachTestTab(gClient, "test-1", function(reply, tabClient) {
      test_attach(tabClient);
    });
  });
  do_test_pending();
}

function test_attach(tabClient) {
  tabClient.attachThread({}, function(response, threadClient) {
    Assert.equal(threadClient.state, "paused");
    threadClient.resume(cleanup);
  });
}

function cleanup() {
  gClient.addListener("closed", function(event) {
    do_test_finished();
  });
  gClient.close();
}
