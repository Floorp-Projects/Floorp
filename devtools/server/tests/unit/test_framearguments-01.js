/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's arguments property.
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
    let args = packet.frame.arguments;
    do_check_eq(args.length, 6);
    do_check_eq(args[0], 42);
    do_check_eq(args[1], true);
    do_check_eq(args[2], "nasu");
    do_check_eq(args[3].type, "null");
    do_check_eq(args[4].type, "undefined");
    do_check_eq(args[5].type, "object");
    do_check_eq(args[5].class, "Object");
    do_check_true(!!args[5].actor);

    gThreadClient.resume(function () {
      finishClient(gClient);
    });
  });

  gDebuggee.eval("(" + function () {
    function stopMe(number, bool, string, null_, undef, object) {
      debugger;
    }
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
}
