/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

var gClient;
var gDebuggee;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-1");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(function (type, traits) {
    attachTestTab(gClient, "test-1", test_attach);
  });
  do_test_pending();
}

function test_attach(response, tabClient) {
  tabClient.attachThread({}, function (response, threadClient) {
    Assert.equal(threadClient.paused, true);
    threadClient.resume(function () {
      test_interrupt(threadClient);
    });
  });
}

function test_interrupt(threadClient) {
  Assert.equal(threadClient.paused, false);
  threadClient.interrupt(function (response) {
    Assert.equal(threadClient.paused, true);
    threadClient.resume(function () {
      Assert.equal(threadClient.paused, false);
      cleanup();
    });
  });
}

function cleanup() {
  gClient.addListener("closed", function (event) {
    do_test_finished();
  });
  gClient.close();
}

