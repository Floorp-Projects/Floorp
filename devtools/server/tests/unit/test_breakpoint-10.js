/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line with multiple entry points
 * triggers no matter which entry point we reach.
 */

add_task(
  threadClientTest(({ threadClient, client, debuggee }) => {
    return new Promise(resolve => {
      threadClient.once("paused", async function(packet) {
        const source = await getSourceById(
          threadClient,
          packet.frame.where.actor
        );
        const location = {
          sourceUrl: source.url,
          line: debuggee.line0 + 3,
          column: 5,
        };

        threadClient.setBreakpoint(location, {});
        await client.waitForRequestsToSettle();

        threadClient.once("paused", async function(packet) {
          // Check the return value.
          Assert.equal(packet.why.type, "breakpoint");
          // Check that the breakpoint worked.
          Assert.equal(debuggee.i, 0);

          // Remove the breakpoint.
          threadClient.removeBreakpoint(location);
          await client.waitForRequestsToSettle();

          const location2 = {
            sourceUrl: source.url,
            line: debuggee.line0 + 3,
            column: 12,
          };
          threadClient.setBreakpoint(location2, {});
          await client.waitForRequestsToSettle();

          threadClient.once("paused", async function(packet) {
            // Check the return value.
            Assert.equal(packet.why.type, "breakpoint");
            // Check that the breakpoint worked.
            Assert.equal(debuggee.i, 1);

            // Remove the breakpoint.
            threadClient.removeBreakpoint(location2);
            await client.waitForRequestsToSettle();

            threadClient.resume().then(resolve);
          });

          // Continue until the breakpoint is hit again.
          await threadClient.resume();
        });

        // Continue until the breakpoint is hit.
        await threadClient.resume();
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
  })
);
