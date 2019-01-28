/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line with multiple entry points
 * triggers no matter which entry point we reach.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );
      const location = { line: debuggee.line0 + 3 };

      source.setBreakpoint(location).then(function([response, bpClient]) {
        // actualLocation is not returned when breakpoints don't skip forward.
        Assert.equal(response.actualLocation, undefined);

        threadClient.addOneTimeListener("paused", function(event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(debuggee.i, 0);

          threadClient.addOneTimeListener("paused", function(event, packet) {
            // Check the return value.
            Assert.equal(packet.type, "paused");
            Assert.equal(packet.why.type, "breakpoint");
            Assert.equal(packet.why.actors[0], bpClient.actor);
            // Check that the breakpoint worked.
            Assert.equal(debuggee.i, 1);

            // Remove the breakpoint.
            bpClient.remove(function(response) {
              threadClient.resume(resolve);
            });
          });

          // Continue until the breakpoint is hit again.
          threadClient.resume();
        });
        // Continue until the breakpoint is hit.
        threadClient.resume();
      });
    });

    /* eslint-disable */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "debugger;\n" +                      // line0 + 1
                     "var a, i = 0;\n" +                  // line0 + 2
                     "for (i = 1; i <= 2; i++) {\n" +     // line0 + 3
                     "  a = i;\n" +                       // line0 + 4
                     "}\n",                               // line0 + 5
                     debuggee);
    /* eslint-enable */
  });
}));
