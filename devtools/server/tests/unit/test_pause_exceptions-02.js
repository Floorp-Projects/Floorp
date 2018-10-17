/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true when the debugger isn't in a
 * paused state will not cause the debuggee to pause when an exceptions is thrown.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.pauseOnExceptions(true, false, function() {
    gThreadClient.addOneTimeListener("paused", function(event, packet) {
      Assert.equal(packet.why.type, "debuggerStatement");
      Assert.equal(packet.frame.where.line, 8);
      gThreadClient.resume(() => finishClient(gClient));
    });

    /* eslint-disable */
    gDebuggee.eval("(" + function () {   // 1
      function stopMe() {                // 2
        throw 42;                        // 3
      }                                  // 4
      try {                              // 5
        stopMe();                        // 6
      } catch (e) {                      // 7
        debugger;                        // 8
      }                                  // 9
    } + ")()");
    /* eslint-enable */
  });
}
