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
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
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
    const args = packet.frame.arguments;
    Assert.equal(args.length, 6);
    Assert.equal(args[0], 42);
    Assert.equal(args[1], true);
    Assert.equal(args[2], "nasu");
    Assert.equal(args[3].type, "null");
    Assert.equal(args[4].type, "undefined");
    Assert.equal(args[5].type, "object");
    Assert.equal(args[5].class, "Object");
    Assert.ok(!!args[5].actor);

    gThreadClient.resume().then(function() {
      finishClient(gClient);
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(number, bool, string, null_, undef, object) {
      debugger;
    }
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
}
