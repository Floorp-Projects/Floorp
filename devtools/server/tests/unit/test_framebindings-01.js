/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check a frame actor's bindings property.
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
    const bindings = packet.frame.environment.bindings;
    const args = bindings.arguments;
    const vars = bindings.variables;

    Assert.equal(args.length, 6);
    Assert.equal(args[0].number.value, 42);
    Assert.equal(args[1].bool.value, true);
    Assert.equal(args[2].string.value, "nasu");
    Assert.equal(args[3].null_.value.type, "null");
    Assert.equal(args[4].undef.value.type, "undefined");
    Assert.equal(args[5].object.value.type, "object");
    Assert.equal(args[5].object.value.class, "Object");
    Assert.ok(!!args[5].object.value.actor);

    Assert.equal(vars.a.value, 1);
    Assert.equal(vars.b.value, true);
    Assert.equal(vars.c.value.type, "object");
    Assert.equal(vars.c.value.class, "Object");
    Assert.ok(!!vars.c.value.actor);

    const objClient = gThreadClient.pauseGrip(vars.c.value);
    objClient.getPrototypeAndProperties(function(response) {
      Assert.equal(response.ownProperties.a.configurable, true);
      Assert.equal(response.ownProperties.a.enumerable, true);
      Assert.equal(response.ownProperties.a.writable, true);
      Assert.equal(response.ownProperties.a.value, "a");

      Assert.equal(response.ownProperties.b.configurable, true);
      Assert.equal(response.ownProperties.b.enumerable, true);
      Assert.equal(response.ownProperties.b.writable, true);
      Assert.equal(response.ownProperties.b.value.type, "undefined");
      Assert.equal(false, "class" in response.ownProperties.b.value);

      gThreadClient.resume().then(function() {
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
