/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that sealed objects report themselves as sealed in their
 * grip.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-grips", server);
  gDebuggee.eval(function stopMe(arg1, arg2) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_object_grip();
                           });
  });
}

function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let obj1 = packet.frame.arguments[0];
    Assert.ok(obj1.sealed);

    let obj1Client = gThreadClient.pauseGrip(obj1);
    Assert.ok(obj1Client.isSealed);

    let obj2 = packet.frame.arguments[1];
    Assert.ok(!obj2.sealed);

    let obj2Client = gThreadClient.pauseGrip(obj2);
    Assert.ok(!obj2Client.isSealed);

    gThreadClient.resume(_ => {
      gClient.close().then(gCallback);
    });
  });

  gDebuggee.eval("(" + function () {
    let obj1 = {};
    Object.seal(obj1);
    stopMe(obj1, {});
  } + "())");
}

