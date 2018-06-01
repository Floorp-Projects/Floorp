/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that requesting a pause actor for the same value multiple
 * times returns the same actor.
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
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const args = packet.frame.arguments;
    const objActor1 = args[0].actor;

    gThreadClient.getFrames(0, 1, function(response) {
      const frame = response.frames[0];
      Assert.equal(objActor1, frame.arguments[0].actor);
      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(obj) {
      debugger;
    }
    stopMe({ foo: "bar" });
  } + ")()");
}
