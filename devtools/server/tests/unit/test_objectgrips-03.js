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
    objClient.getProperty("x", function(response) {
      Assert.equal(response.descriptor.configurable, true);
      Assert.equal(response.descriptor.enumerable, true);
      Assert.equal(response.descriptor.writable, true);
      Assert.equal(response.descriptor.value, 10);

      objClient.getProperty("y", function(response) {
        Assert.equal(response.descriptor.configurable, true);
        Assert.equal(response.descriptor.enumerable, true);
        Assert.equal(response.descriptor.writable, true);
        Assert.equal(response.descriptor.value, "kaiju");

        objClient.getProperty("a", function(response) {
          Assert.equal(response.descriptor.configurable, true);
          Assert.equal(response.descriptor.enumerable, true);
          Assert.equal(response.descriptor.get.type, "object");
          Assert.equal(response.descriptor.get.class, "Function");
          Assert.equal(response.descriptor.set.type, "undefined");

          gThreadClient.resume(function() {
            gClient.close().then(gCallback);
          });
        });
      });
    });
  });

  gDebuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
}

