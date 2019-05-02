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

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const env = packet.frame.environment;
    Assert.notEqual(env, undefined);

    const objClient = gThreadClient.pauseGrip(env.object);
    objClient.getPrototypeAndProperties(function(response) {
      Assert.equal(response.ownProperties.PI.value, Math.PI);
      Assert.equal(response.ownProperties.cos.value.type, "object");
      Assert.equal(response.ownProperties.cos.value.class, "Function");
      Assert.ok(!!response.ownProperties.cos.value.actor);

      // Skip the global lexical scope.
      const parentEnv = env.parent.parent;
      Assert.notEqual(parentEnv, undefined);

      const parentClient = gThreadClient.pauseGrip(parentEnv.object);
      parentClient.getPrototypeAndProperties(function(response) {
        Assert.equal(response.ownProperties.a.value, Math.PI * 100);
        Assert.equal(response.ownProperties.r.value, 10);
        Assert.equal(response.ownProperties.Object.value.type, "object");
        Assert.equal(response.ownProperties.Object.value.class, "Function");
        Assert.ok(!!response.ownProperties.Object.value.actor);

        gThreadClient.resume().then(function() {
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
