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
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, "bar");
      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });
    gThreadClient.pauseOnExceptions(true, true);
    gThreadClient.resume();
  });

  try {
    /* eslint-disable */
    gDebuggee.eval("(" + function () {
      debugger;
      try {
        throw "foo";
      } catch (e) {}
      throw "bar";
    } + ")()");
    /* eslint-enable */
  } catch (e) {
    /* Empty */
  }
}
