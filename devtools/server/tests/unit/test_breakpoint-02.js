/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

add_task(
  threadFrontTest(({ threadFront, client, debuggee }) => {
    return new Promise(resolve => {
      threadFront.once("paused", async function(packet) {
        const source = await getSourceById(
          threadFront,
          packet.frame.where.actor
        );
        const location = { sourceUrl: source.url, line: debuggee.line0 + 3 };

        await threadFront.resume();

        // Setting the breakpoint later should interrupt the debuggee.
        threadFront.once("paused", function(packet) {
          Assert.equal(packet.why.type, "interrupted");
        });

        threadFront.setBreakpoint(location, {});
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
