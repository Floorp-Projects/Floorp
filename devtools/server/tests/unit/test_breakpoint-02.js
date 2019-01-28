/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise((resolve) => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const location = { line: debuggee.line0 + 3 };
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );

      threadClient.resume();

      // Setting the breakpoint later should interrupt the debuggee.
      threadClient.addOneTimeListener("paused", function(event, packet) {
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.why.type, "interrupted");
      });

      source.setBreakpoint(location).then(function() {
        executeSoon(resolve);
      }, function(response) {
        // Eval scripts don't stick around long enough for the breakpoint to be set,
        // so just make sure we got the expected response from the actor.
        Assert.notEqual(response.error, "noScript");

        executeSoon(resolve);
      });
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
}));
