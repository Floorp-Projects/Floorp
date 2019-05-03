/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Verify that frame actors retrieved with the frames request
 * are included in the pause packet's popped-frames property.
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
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    gThreadClient.getFrames(0, null).then(function(frameResponse) {
      Assert.equal(frameResponse.frames.length, 5);
      // Now wait for the next pause, after which the three
      // youngest actors should be popped..
      const expectPopped = frameResponse.frames.slice(0, 3).map(frame => frame.actor);
      expectPopped.sort();

      gThreadClient.addOneTimeListener("paused", function(event, pausePacket) {
        const popped = pausePacket.poppedFrames.sort();
        Assert.equal(popped.length, 3);
        for (let i = 0; i < 3; i++) {
          Assert.equal(expectPopped[i], popped[i]);
        }

        gThreadClient.resume().then(() => finishClient(gClient));
      });
      gThreadClient.resume();
    });
  });

  gDebuggee.eval("(" + function() {
    function depth3() {
      debugger;
    }
    function depth2() {
      depth3();
    }
    function depth1() {
      depth2();
    }
    depth1();
    debugger;
  } + ")()");
}
