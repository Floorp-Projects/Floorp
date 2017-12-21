/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check pauses within evals.
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
                             test_pauses_eval();
                           });
  });
  do_test_pending();
}

function test_pauses_eval() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.eval(null, "debugger", function (response) {
      // Expect a resume then a debuggerStatement pause.
      Assert.equal(response.type, "resumed");
      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        Assert.equal(packet.why.type, "debuggerStatement");
        // Resume from the debugger statement should immediately re-pause
        // with a clientEvaluated reason.
        gThreadClient.resume(function (packet) {
          Assert.equal(packet.type, "resumed");
          gThreadClient.addOneTimeListener("paused", function (event, packet) {
            Assert.equal(packet.why.type, "clientEvaluated");
            gThreadClient.resume(function () {
              finishClient(gClient);
            });
          });
        });
      });
    });
  });
  gDebuggee.eval("(" + function () {
    function stopMe(arg) {
      debugger;
    }
    stopMe();
  } + ")()");
}
