/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that removing a breakpoint works.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    let done = false;
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );
      const location = { sourceUrl: source.url, line: debuggee.line0 + 2 };

      threadClient.setBreakpoint(location, {});
      threadClient.addOneTimeListener("paused", function(event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.frame.where.actor, source.actorID);
        Assert.equal(packet.frame.where.line, location.line);
        Assert.equal(packet.why.type, "breakpoint");
        // Check that the breakpoint worked.
        Assert.equal(debuggee.a, undefined);

        // Remove the breakpoint.
        threadClient.removeBreakpoint(location);
        done = true;
        threadClient.addOneTimeListener("paused",
                                        function(event, packet) {
              // The breakpoint should not be hit again.
                                          threadClient.resume().then(function() {
                                            Assert.ok(false);
                                          });
                                        });
        threadClient.resume();
      });

      // Continue until the breakpoint is hit.
      threadClient.resume();
    });

    /* eslint-disable */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "function foo(stop) {\n" + // line0 + 1
                     "  this.a = 1;\n" +        // line0 + 2
                     "  if (stop) return;\n" +  // line0 + 3
                     "  delete this.a;\n" +     // line0 + 4
                     "  foo(true);\n" +         // line0 + 5
                     "}\n" +                    // line0 + 6
                     "debugger;\n" +            // line1 + 7
                     "foo();\n",                // line1 + 8
                     debuggee);
    /* eslint-enable */
    if (!done) {
      Assert.ok(false);
    }
    resolve();
  });
}));
