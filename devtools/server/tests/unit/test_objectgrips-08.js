/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  gDebuggee.eval(function stopMe(arg1) {
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
    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Object");

    const objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function(response) {
      Assert.equal(response.ownProperties.a.configurable, true);
      Assert.equal(response.ownProperties.a.enumerable, true);
      Assert.equal(response.ownProperties.a.writable, true);
      Assert.equal(response.ownProperties.a.value.type, "Infinity");

      Assert.equal(response.ownProperties.b.configurable, true);
      Assert.equal(response.ownProperties.b.enumerable, true);
      Assert.equal(response.ownProperties.b.writable, true);
      Assert.equal(response.ownProperties.b.value.type, "-Infinity");

      Assert.equal(response.ownProperties.c.configurable, true);
      Assert.equal(response.ownProperties.c.enumerable, true);
      Assert.equal(response.ownProperties.c.writable, true);
      Assert.equal(response.ownProperties.c.value.type, "NaN");

      Assert.equal(response.ownProperties.d.configurable, true);
      Assert.equal(response.ownProperties.d.enumerable, true);
      Assert.equal(response.ownProperties.d.writable, true);
      Assert.equal(response.ownProperties.d.value.type, "-0");

      gThreadClient.resume(function() {
        gClient.close().then(gCallback);
      });
    });
  });

  gDebuggee.eval("stopMe({ a: Infinity, b: -Infinity, c: NaN, d: -0 })");
}

