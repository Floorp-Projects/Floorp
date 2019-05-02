/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that setting a breakpoint twice in a line without bytecodes works
 * as expected.
 */

const NUM_BREAKPOINTS = 10;
var gBpActor;
var gCount;

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );
      const location = { line: debuggee.line0 + 3};

      source.setBreakpoint(location).then(function([response, bpClient]) {
        // Check that the breakpoint has properly skipped forward one line.
        Assert.equal(response.actualLocation.source.actor, source.actor);
        Assert.equal(response.actualLocation.line, location.line + 1);
        gBpActor = response.actor;

        // Set more breakpoints at the same location.
        set_breakpoints(source, location);
      });
    });

    /* eslint-disable no-multi-spaces */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "function foo() {\n" + // line0 + 1
                     "  this.a = 1;\n" +    // line0 + 2
                     "  // A comment.\n" +  // line0 + 3
                     "  this.b = 2;\n" +    // line0 + 4
                     "}\n" +                // line0 + 5
                     "debugger;\n" +        // line0 + 6
                     "foo();\n",            // line0 + 7
                     debuggee);
    /* eslint-enable no-multi-spaces */

    // Set many breakpoints at the same location.
    function set_breakpoints(source, location) {
      Assert.notEqual(gCount, NUM_BREAKPOINTS);
      source.setBreakpoint(location).then(function([response, bpClient]) {
        // Check that the breakpoint has properly skipped forward one line.
        Assert.equal(response.actualLocation.source.actor, source.actor);
        Assert.equal(response.actualLocation.line, location.line + 1);
        // Check that the same breakpoint actor was returned.
        Assert.equal(response.actor, gBpActor);

        if (++gCount < NUM_BREAKPOINTS) {
          set_breakpoints(source, location);
          return;
        }

        // After setting all the breakpoints, check that only one has effectively
        // remained.
        threadClient.addOneTimeListener("paused", function(event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.frame.where.actor, source.actor);
          Assert.equal(packet.frame.where.line, location.line + 1);
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(debuggee.a, 1);
          Assert.equal(debuggee.b, undefined);

          threadClient.addOneTimeListener("paused", function(event, packet) {
            // We don't expect any more pauses after the breakpoint was hit once.
            Assert.ok(false);
          });
          threadClient.resume().then(function() {
            // Give any remaining breakpoints a chance to trigger.
            do_timeout(1000, resolve);
          });
        });
        // Continue until the breakpoint is hit.
        threadClient.resume();
      });
    }
  });
}));
