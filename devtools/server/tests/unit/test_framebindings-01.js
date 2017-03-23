/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's bindings property.
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
    let bindings = packet.frame.environment.bindings;
    let args = bindings.arguments;
    let vars = bindings.variables;

    do_check_eq(args.length, 6);
    do_check_eq(args[0].number.value, 42);
    do_check_eq(args[1].bool.value, true);
    do_check_eq(args[2].string.value, "nasu");
    do_check_eq(args[3].null_.value.type, "null");
    do_check_eq(args[4].undef.value.type, "undefined");
    do_check_eq(args[5].object.value.type, "object");
    do_check_eq(args[5].object.value.class, "Object");
    do_check_true(!!args[5].object.value.actor);

    do_check_eq(vars.a.value, 1);
    do_check_eq(vars.b.value, true);
    do_check_eq(vars.c.value.type, "object");
    do_check_eq(vars.c.value.class, "Object");
    do_check_true(!!vars.c.value.actor);

    let objClient = gThreadClient.pauseGrip(vars.c.value);
    objClient.getPrototypeAndProperties(function (response) {
      do_check_eq(response.ownProperties.a.configurable, true);
      do_check_eq(response.ownProperties.a.enumerable, true);
      do_check_eq(response.ownProperties.a.writable, true);
      do_check_eq(response.ownProperties.a.value, "a");

      do_check_eq(response.ownProperties.b.configurable, true);
      do_check_eq(response.ownProperties.b.enumerable, true);
      do_check_eq(response.ownProperties.b.writable, true);
      do_check_eq(response.ownProperties.b.value.type, "undefined");
      do_check_false("class" in response.ownProperties.b.value);

      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(number, bool, string, null_, undef, object) {
      var a = 1;
      var b = true;
      var c = { a: "a", b: undefined };
      debugger;
    }
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
  /* eslint-enable */
}
