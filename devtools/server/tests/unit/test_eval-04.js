/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check evals against different frames.
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
                             test_different_frames_eval();
                           });
  });
  do_test_pending();
}

function test_different_frames_eval() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    gThreadClient.getFrames(0, 2, function (response) {
      let frame0 = response.frames[0];
      let frame1 = response.frames[1];

      // Eval against the top frame...
      gThreadClient.eval(frame0.actor, "arg", function (response) {
        Assert.equal(response.type, "resumed");
        gThreadClient.addOneTimeListener("paused", function (event, packet) {
          // 'arg' should have been evaluated in frame0
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "clientEvaluated");
          Assert.equal(packet.why.frameFinished.return, "arg0");

          // Now eval against the second frame.
          gThreadClient.eval(frame1.actor, "arg", function (response) {
            gThreadClient.addOneTimeListener("paused", function (event, packet) {
              // 'arg' should have been evaluated in frame1
              Assert.equal(packet.type, "paused");
              Assert.equal(packet.why.frameFinished.return, "arg1");

              gThreadClient.resume(function () {
                finishClient(gClient);
              });
            });
          });
        });
      });
    });
  });

  gDebuggee.eval("(" + function () {
    function frame0(arg) {
      debugger;
    }
    function frame1(arg) {
      frame0("arg0");
    }
    frame1("arg1");
  } + ")()");
}
