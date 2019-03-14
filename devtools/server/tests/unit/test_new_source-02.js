/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that sourceURL has the correct effect when using gThreadClient.eval.
 */

var gDebuggee;
var gClient;
var gTargetFront;
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
                             gTargetFront = targetFront;
                             test_simple_new_source();
                           });
  });
  do_test_pending();
}

function test_simple_new_source() {
  gThreadClient.addOneTimeListener("paused", function() {
    gThreadClient.addOneTimeListener("newSource", async function(event2, packet2) {
      // The "stopMe" eval source is emitted first.
      Assert.equal(event2, "newSource");
      Assert.equal(packet2.type, "newSource");
      Assert.ok(!!packet2.source);
      Assert.ok(packet2.source.introductionType, "eval");

      gThreadClient.addOneTimeListener("newSource", function(event, packet) {
        Assert.equal(event, "newSource");
        Assert.equal(packet.type, "newSource");
        dump(JSON.stringify(packet, null, 2));
        Assert.ok(!!packet.source);
        Assert.ok(!!packet.source.url.match(/example\.com/));

        finishClient(gClient);
      });

      const consoleFront = await gTargetFront.getFront("console");
      consoleFront.evaluateJSAsync("function f() { }\n//# sourceURL=http://example.com/code.js");
    });
  });

  /* eslint-disable */
  gDebuggee.eval("(" + function () {
    function stopMe(arg1) { debugger; }
    stopMe({obj: true});
  } + ")()");
  /* eslint-enable */
}
