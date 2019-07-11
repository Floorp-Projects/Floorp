/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that setting pauseOnExceptions to true when the debugger isn't in a
 * paused state will not cause the debuggee to pause when an exceptions is thrown.
 */

var gDebuggee;
var gClient;
var gThreadFront;

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      test_pause_frame();
    });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadFront.pauseOnExceptions(true, false).then(function() {
    gThreadFront.once("paused", function(packet) {
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, 42);
      gThreadFront.resume().then(() => finishClient(gClient));
    });

    /* eslint-disable */
    gDebuggee.eval("(" + function () {   // 1
      function stopMe() {                // 2
        throw 42;                        // 3
      }                                  // 4
      try {                              // 5
        stopMe();                        // 6
      } catch (e) {}                     // 7
    } + ")()");
    /* eslint-enable */
  });
}
