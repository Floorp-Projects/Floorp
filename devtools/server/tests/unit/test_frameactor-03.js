/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Verify that a frame actor is properly expired when the frame goes away.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
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
  gThreadClient.addOneTimeListener("paused", function(event, packet1) {
    gThreadClient.addOneTimeListener("paused", function(event, packet2) {
      const poppedFrames = packet2.poppedFrames;
      Assert.equal(typeof (poppedFrames), typeof ([]));
      Assert.ok(poppedFrames.includes(packet1.frame.actor));
      gThreadClient.resume().then(function() {
        finishClient(gClient);
      });
    });
    gThreadClient.resume();
  });

  gDebuggee.eval("(" + function() {
    function stopMe() {
      debugger;
    }
    stopMe();
    debugger;
  } + ")()");
}
