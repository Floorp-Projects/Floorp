/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

add_task(
  threadClientTest(({ threadClient, client, debuggee }) => {
    return new Promise(resolve => {
      threadClient.once("paused", async function(packet) {
        const source = await getSourceById(
          threadClient,
          packet.frame.where.actor
        );
        const location = { sourceUrl: source.url, line: debuggee.line0 + 3 };

        await threadClient.resume();

        // Setting the breakpoint later should interrupt the debuggee.
        threadClient.once("paused", function(packet) {
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "interrupted");
        });

        threadClient.setBreakpoint(location, {});
        await client.waitForRequestsToSettle();
        executeSoon(resolve);
      });

      /* eslint-disable */
    Cu.evalInSandbox(
      "var line0 = Error().lineNumber;\n" +
      "debugger;\n" +
      "var a = 1;\n" +  // line0 + 2
      "var b = 2;\n",  // line0 + 3
      debuggee
    );
    /* eslint-enable */
    });
  })
);
