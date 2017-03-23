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
    do_check_eq(threadClient.paused, true);
    threadClient.resume(function () {
      test_interrupt(threadClient);
    });
  });
}

function test_interrupt(threadClient) {
  do_check_eq(threadClient.paused, false);
  threadClient.interrupt(function (response) {
    do_check_eq(threadClient.paused, true);
    threadClient.resume(function () {
      do_check_eq(threadClient.paused, false);
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

