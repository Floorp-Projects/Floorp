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
    objClient.getPrototype(function(response) {
      Assert.ok(response.prototype != undefined);

      const protoClient = gThreadClient.pauseGrip(response.prototype);
      protoClient.getOwnPropertyNames(function(response) {
        Assert.equal(response.ownPropertyNames.length, 2);
        Assert.equal(response.ownPropertyNames[0], "b");
        Assert.equal(response.ownPropertyNames[1], "c");

        gThreadClient.resume(function() {
          gClient.close().then(gCallback);
        });
      });
    });
  });

  gDebuggee.eval(function Constr() {
    this.a = 1;
  }.toString());
  gDebuggee.eval(
    "Constr.prototype = { b: true, c: 'foo' }; var o = new Constr(); stopMe(o)");
}

