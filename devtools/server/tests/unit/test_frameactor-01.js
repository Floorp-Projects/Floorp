/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we get a frame actor along with a debugger statement.
 */

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
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
    Assert.ok(!!packet.frame);
    Assert.ok(!!packet.frame.actor);
    Assert.equal(packet.frame.displayName, "stopMe");
    gThreadFront.resume().then(function() {
      finishClient(gClient);
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe() {
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
