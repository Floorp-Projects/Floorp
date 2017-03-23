/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check the environment bindings of a |with| in global scope.
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
    let env = packet.frame.environment;
    do_check_neq(env, undefined);

    let objClient = gThreadClient.pauseGrip(env.object);
    objClient.getPrototypeAndProperties(function (response) {
      do_check_eq(response.ownProperties.PI.value, Math.PI);
      do_check_eq(response.ownProperties.cos.value.type, "object");
      do_check_eq(response.ownProperties.cos.value.class, "Function");
      do_check_true(!!response.ownProperties.cos.value.actor);

      // Skip the global lexical scope.
      let parentEnv = env.parent.parent;
      do_check_neq(parentEnv, undefined);

      let parentClient = gThreadClient.pauseGrip(parentEnv.object);
      parentClient.getPrototypeAndProperties(function (response) {
        do_check_eq(response.ownProperties.a.value, Math.PI * 100);
        do_check_eq(response.ownProperties.r.value, 10);
        do_check_eq(response.ownProperties.Object.value.type, "object");
        do_check_eq(response.ownProperties.Object.value.class, "Function");
        do_check_true(!!response.ownProperties.Object.value.actor);

        gThreadClient.resume(function () {
          finishClient(gClient);
        });
      });
    });
  });

  gDebuggee.eval("var a, r = 10;\n" +
                 "with (Math) {\n" +
                 "  a = PI * r * r;\n" +
                 "  debugger;\n" +
                 "}");
}
