/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * If pauseOnExceptions is checked, when condition throws,
 * make sure conditional breakpoint pauses.
 */

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-conditional-breakpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-conditional-breakpoint", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      test_simple_breakpoint();
    });
  });
  do_test_pending();
}

function test_simple_breakpoint() {
  gThreadFront.once("paused", async function(packet) {
    const source = await getSourceById(gThreadFront, packet.frame.where.actor);

    gThreadFront.pauseOnExceptions(true, false);
    const location = { sourceUrl: source.url, line: 3 };
    gThreadFront.setBreakpoint(location, { condition: "throw new Error()" });
    gThreadFront.once("paused", async function(packet) {
      // Check the return value.
      Assert.equal(packet.why.type, "exception");
      Assert.equal(packet.frame.where.line, 1);

      // Step over twice.
      await stepOver(gThreadFront);
      packet = await stepOver(gThreadFront);

      // Check the return value.
      Assert.equal(packet.why.type, "breakpointConditionThrown");
      Assert.equal(packet.frame.where.line, 3);

      // Remove the breakpoint.
      gThreadFront.removeBreakpoint(location);

      gThreadFront.resume().then(function() {
        finishClient(gClient);
      });
    });

    // Continue until the breakpoint is hit.
    gThreadFront.resume();
  });

  /* eslint-disable */
  Cu.evalInSandbox("debugger;\n" +   // 1
                   "var a = 1;\n" +  // 2
                   "var b = 2;\n",  // 3
                   gDebuggee,
                   "1.8",
                   "test.js",
                   1);
  /* eslint-enable */
}
