/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that we get a frame actor along with a debugger statement.
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
    Assert.ok(!!packet.frame);
    Assert.ok(!!packet.frame.actor);
    Assert.equal(packet.frame.callee.name, "stopMe");
    gThreadClient.resume(function () {
      finishClient(gClient);
    });
  });

  gDebuggee.eval("(" + function () {
    function stopMe() {
      debugger;
    }
    stopMe();
  } + ")()");
}
