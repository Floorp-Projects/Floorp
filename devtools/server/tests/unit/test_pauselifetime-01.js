/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that pause-lifetime grips go away correctly after a resume.
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
    let pauseActor = packet.actor;

    // Make a bogus request to the pause-lifetime actor.  Should get
    // unrecognized-packet-type (and not no-such-actor).
    gClient.request({ to: pauseActor, type: "bogusRequest" }, function (response) {
      Assert.equal(response.error, "unrecognizedPacketType");

      gThreadClient.resume(function () {
        // Now that we've resumed, should get no-such-actor for the
        // same request.
        gClient.request({ to: pauseActor, type: "bogusRequest" }, function (response) {
          Assert.equal(response.error, "noSuchActor");
          finishClient(gClient);
        });
      });
    });
  });

  gDebuggee.eval("(" + function () {
    function stopMe() {
      debugger;
    }
    stopMe();
  } + ")()");
}
