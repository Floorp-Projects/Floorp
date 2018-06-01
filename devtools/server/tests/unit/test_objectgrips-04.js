/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

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
      Assert.equal(response.ownProperties.x.configurable, true);
      Assert.equal(response.ownProperties.x.enumerable, true);
      Assert.equal(response.ownProperties.x.writable, true);
      Assert.equal(response.ownProperties.x.value, 10);

      Assert.equal(response.ownProperties.y.configurable, true);
      Assert.equal(response.ownProperties.y.enumerable, true);
      Assert.equal(response.ownProperties.y.writable, true);
      Assert.equal(response.ownProperties.y.value, "kaiju");

      Assert.equal(response.ownProperties.a.configurable, true);
      Assert.equal(response.ownProperties.a.enumerable, true);
      Assert.equal(response.ownProperties.a.get.type, "object");
      Assert.equal(response.ownProperties.a.get.class, "Function");
      Assert.equal(response.ownProperties.a.set.type, "undefined");

      Assert.ok(response.prototype != undefined);

      const protoClient = gThreadClient.pauseGrip(response.prototype);
      protoClient.getOwnPropertyNames(function(response) {
        Assert.ok(response.ownPropertyNames.toString != undefined);

        gThreadClient.resume(function() {
          gClient.close().then(gCallback);
        });
      });
    });
  });

  gDebuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
}

