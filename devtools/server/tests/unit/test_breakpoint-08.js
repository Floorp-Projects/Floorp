/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line without code in a child script
 * will skip forward, in a file with two scripts.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const line = debuggee.line0 + 3;
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );

      // this test has been disabled for a long time so the functionality doesn't work
      const response = await threadClient
        .setBreakpoint({ sourceUrl: source.url, line: line }, {});
      // check that the breakpoint has properly skipped forward one line.
      assert.equal(response.actuallocation.source.actor, source.actor);
      Assert.equal(response.actualLocation.line, location.line + 1);

      threadClient.addOneTimeListener("paused", function(event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.frame.where.actor, source.actor);
        Assert.equal(packet.frame.where.line, location.line + 1);
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.why.actors[0], response.bpClient.actor);
        // Check that the breakpoint worked.
        Assert.equal(debuggee.a, 1);
        Assert.equal(debuggee.b, undefined);

        // Remove the breakpoint.
        response.bpClient.remove(function(response) {
          threadClient.resume().then(resolve);
        });
      });

      // Continue until the breakpoint is hit.
      threadClient.resume();
    });

    /* eslint-disable */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "function foo() {\n" + // line0 + 1
                     "  this.a = 1;\n" +    // line0 + 2
                     "  // A comment.\n" +  // line0 + 3
                     "  this.b = 2;\n" +    // line0 + 4
                     "}\n",                 // line0 + 5
                     debuggee,
                     "1.7",
                     "script1.js");

    Cu.evalInSandbox("var line1 = Error().lineNumber;\n" +
                     "debugger;\n" +        // line1 + 1
                     "foo();\n",           // line1 + 2
                     debuggee,
                     "1.7",
                     "script2.js");
    /* eslint-enable */
  });
}));
