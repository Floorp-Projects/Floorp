/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's parent bindings.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    let parentEnv = packet.frame.environment.parent;
    const bindings = parentEnv.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;
    Assert.notEqual(parentEnv, undefined);
    Assert.equal(args.length, 0);
    Assert.equal(vars.stopMe.value.type, "object");
    Assert.equal(vars.stopMe.value.class, "Function");
    Assert.ok(!!vars.stopMe.value.actor);

    // Skip the global lexical scope.
    parentEnv = parentEnv.parent.parent;
    Assert.notEqual(parentEnv, undefined);
    const objClient = gThreadClient.pauseGrip(parentEnv.object);
    objClient.getPrototypeAndProperties(function(response) {
      Assert.equal(response.ownProperties.Object.value.type, "object");
      Assert.equal(response.ownProperties.Object.value.class, "Function");
      Assert.ok(!!response.ownProperties.Object.value.actor);

      gThreadClient.resume(function() {
        finishClient(gClient);
      });
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(number, bool, string, null_, undef, object) {
      var a = 1;
      var b = true;
      var c = { a: "a" };
      debugger;
    }
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
}
