/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  gDebuggee.eval(function stopMe(arg1) {
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
    let args = packet.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function (response) {
      do_check_eq(response.ownProperties.a.configurable, true);
      do_check_eq(response.ownProperties.a.enumerable, true);
      do_check_eq(response.ownProperties.a.writable, true);
      do_check_eq(response.ownProperties.a.value.type, "Infinity");

      do_check_eq(response.ownProperties.b.configurable, true);
      do_check_eq(response.ownProperties.b.enumerable, true);
      do_check_eq(response.ownProperties.b.writable, true);
      do_check_eq(response.ownProperties.b.value.type, "-Infinity");

      do_check_eq(response.ownProperties.c.configurable, true);
      do_check_eq(response.ownProperties.c.enumerable, true);
      do_check_eq(response.ownProperties.c.writable, true);
      do_check_eq(response.ownProperties.c.value.type, "NaN");

      do_check_eq(response.ownProperties.d.configurable, true);
      do_check_eq(response.ownProperties.d.enumerable, true);
      do_check_eq(response.ownProperties.d.writable, true);
      do_check_eq(response.ownProperties.d.value.type, "-0");

      gThreadClient.resume(function () {
        gClient.close().then(gCallback);
      });
    });
  });

  gDebuggee.eval("stopMe({ a: Infinity, b: -Infinity, c: NaN, d: -0 })");
}

