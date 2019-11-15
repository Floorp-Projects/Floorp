/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check conditional breakpoint when condition evaluates to true.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_simple_breakpoint();
    },
    { waitForFinish: true }
  )
);

function test_simple_breakpoint() {
  let hitBreakpoint = false;

  gThreadFront.once("paused", async function(packet) {
    const source = await getSourceById(gThreadFront, packet.frame.where.actor);
    const location = { sourceUrl: source.url, line: 3 };
    gThreadFront.setBreakpoint(location, { condition: "a === 1" });
    gThreadFront.once("paused", function(packet) {
      Assert.equal(hitBreakpoint, false);
      hitBreakpoint = true;

      // Check the return value.
      Assert.equal(packet.why.type, "breakpoint");
      Assert.equal(packet.frame.where.line, 3);

      // Remove the breakpoint.
      gThreadFront.removeBreakpoint(location);

      gThreadFront.resume().then(function() {
        threadFrontTestFinished();
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

  Assert.equal(hitBreakpoint, true);
}
