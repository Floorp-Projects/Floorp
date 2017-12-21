/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true when the debugger isn't in a
 * paused state will cause the debuggee to pause when an exceptions is thrown.
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
  gThreadClient.pauseOnExceptions(true, false, function () {
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, 42);
      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });

    /* eslint-disable */
    gDebuggee.eval("(" + function () {
      function stopMe() {
        throw 42;
      }
      try {
        stopMe();
      } catch (e) {}
    } + ")()");
    /* eslint-enable */
  });
}
