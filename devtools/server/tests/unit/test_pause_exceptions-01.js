/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting pauseOnExceptions to true will cause the debuggee to pause
 * when an exceptions is thrown.
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
  gThreadFront.once("paused", function(packet) {
    gThreadFront.once("paused", function(packet) {
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.why.exception, 42);
      gThreadFront.resume().then(() => finishClient(gClient));
    });

    gThreadFront.pauseOnExceptions(true, false);
    gThreadFront.resume();
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe() {
      debugger;
      throw 42;
    }
    try {
      stopMe();
    } catch (e) {}
  } + ")()");
  /* eslint-enable */
}
