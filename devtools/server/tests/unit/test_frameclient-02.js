/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    // Ask for exactly the number of frames we expect.
    gThreadClient.addOneTimeListener("framesadded", function () {
      Assert.ok(!gThreadClient.moreFrames);
      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });
    Assert.ok(gThreadClient.fillFrames(3));
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    var recurseLeft = 1;
    function recurse() {
      if (--recurseLeft == 0) {
        debugger;
        return;
      }
      recurse();
    }
    recurse();
  } + ")()");
  /* eslint-enable */
}
