/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check basic breakpoint functionality.
 */

add_task(
  threadClientTest(async ({ threadClient, debuggee }) => {
    (async () => {
      info("Wait for the debugger statement to be hit");
      let packet = await waitForPause(threadClient);
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );

      const location = { sourceUrl: source.url, line: debuggee.line0 + 3 };

      threadClient.setBreakpoint(location, {});

      await threadClient.resume();
      packet = await waitForPause(threadClient);

      info("Paused at the breakpoint");
      Assert.equal(packet.type, "paused");
      Assert.equal(packet.frame.where.actor, source.actor);
      Assert.equal(packet.frame.where.line, location.line);
      Assert.equal(packet.why.type, "breakpoint");

      info("Check that the breakpoint worked.");
      Assert.equal(debuggee.a, 1);
      Assert.equal(debuggee.b, undefined);

      await threadClient.resume();
    })();

    /*
     * Be sure to run debuggee code in its own HTML 'task', so that when we call
     * the onDebuggerStatement hook, the test's own microtasks don't get suspended
     * along with the debuggee's.
     */
    do_timeout(0, () => {
    /* eslint-disable */
    Cu.evalInSandbox(
      "var line0 = Error().lineNumber;\n" +
        "debugger;\n" +   // line0 + 1
        "var a = 1;\n" +  // line0 + 2
        "var b = 2;\n",   // line0 + 3
        debuggee
    );
    /* eslint-enable */
    });
  })
);
