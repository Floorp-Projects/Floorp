/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check eval resume/re-pause with a throw.
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
                             test_throw_eval();
                           });
  });
  do_test_pending();
}

function test_throw_eval() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.eval(null, "throw 'failure'", function (response) {
      Assert.equal(response.type, "resumed");
      // Expect a pause notification immediately.
      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // Check the return value...
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.why.type, "clientEvaluated");
        Assert.equal(packet.why.frameFinished.throw, "failure");
        gThreadClient.resume(function () {
          finishClient(gClient);
        });
      });
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(arg1) { debugger; }
    stopMe({obj: true});
  } + ")()");
  /* eslint-enable */
}
