/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check basic breakpoint functionality.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      const source = threadClient.source(packet.frame.where.source);
      const location = {
        line: debuggee.line0 + 3,
      };

      source.setBreakpoint(location).then(function([response, bpClient]) {
        threadClient.addOneTimeListener("paused", function(event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.frame.where.source.actor, source.actor);
          Assert.equal(packet.frame.where.line, location.line);
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(debuggee.a, 1);
          Assert.equal(debuggee.b, undefined);

          // Remove the breakpoint.
          bpClient.remove(function(response) {
            threadClient.resume(resolve);
          });
        });

        // Continue until the breakpoint is hit.
        threadClient.resume();
      });
    });

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
}));
