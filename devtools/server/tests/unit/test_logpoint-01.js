/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that logpoints call console.log.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-logpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-logpoint",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_breakpoint();
                           });
  });
  do_test_pending();
}

function test_simple_breakpoint() {
  gThreadClient.addOneTimeListener("paused", async function(event, packet) {
    const source = await getSourceById(
      gThreadClient,
      packet.frame.where.actor
    );

    // Set a logpoint which should invoke console.log.
    gThreadClient.setBreakpoint({
      sourceUrl: source.url,
      line: 4,
    }, { logValue: "a" });

    // Execute the rest of the code.
    gThreadClient.resume();
  });

  // Sandboxes don't have a console available so we add our own.
  /* eslint-disable */
  Cu.evalInSandbox("var console = { log: v => { this.logValue = v } };\n" + // 1
                   "debugger;\n" + // 2
                   "var a = 'three';\n" +  // 3
                   "var b = 2;\n", // 4
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */

  Assert.equal(gDebuggee.logValue, "three");
  finishClient(gClient);
}
