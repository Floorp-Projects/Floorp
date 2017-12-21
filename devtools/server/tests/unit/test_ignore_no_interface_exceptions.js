/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that the debugger automatically ignores NS_ERROR_NO_INTERFACE
 * exceptions, but not normal ones.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-no-interface");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-no-interface",
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
      function QueryInterface() {
        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
      function stopMe() {
        throw 42;
      }
      try {
        QueryInterface();
      } catch (e) {}
      try {
        stopMe();
      } catch (e) {}
    } + ")()");
    /* eslint-enable */
  });
}
