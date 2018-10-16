/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable max-nested-callbacks */

"use strict";

/**
 * Test that the preview in a Promise's grip is correct when the Promise is
 * pending.
 */

function run_test() {
  initTestDebuggerServer();
  const debuggee = addTestGlobal("test-promise-state");
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect().then(function() {
    attachTestTabAndResume(
      client, "test-promise-state",
      function(response, targetFront, threadClient) {
        (async function() {
          const packet = await executeOnNextTickAndWaitForPause(
            () => evalCode(debuggee), client);

          const grip = packet.frame.environment.bindings.variables.p;
          ok(grip.value.preview);
          equal(grip.value.class, "Promise");
          equal(grip.value.promiseState.state, "pending");

          finishClient(client);
        })();
      });
  });
  do_test_pending();
}

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "doTest();\n" +
    function doTest() {
      var p = new Promise(function () {});
      debugger;
    },
    debuggee
  );
  /* eslint-enable */
}
