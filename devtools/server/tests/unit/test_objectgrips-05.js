/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that frozen objects report themselves as frozen in their
 * grip.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
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
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_object_grip();
                           });
  });
}

function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const obj1 = packet.frame.arguments[0];
    Assert.ok(obj1.frozen);

    const obj1Client = gThreadClient.pauseGrip(obj1);
    Assert.ok(obj1Client.isFrozen);

    const obj2 = packet.frame.arguments[1];
    Assert.ok(!obj2.frozen);

    const obj2Client = gThreadClient.pauseGrip(obj2);
    Assert.ok(!obj2Client.isFrozen);

    gThreadClient.resume(_ => {
      gClient.close().then(gCallback);
    });
  });

  /* eslint-disable no-undef */
  gDebuggee.eval("(" + function() {
    const obj1 = {};
    Object.freeze(obj1);
    stopMe(obj1, {});
  } + "())");
  /* eslint-enable no-undef */
}
