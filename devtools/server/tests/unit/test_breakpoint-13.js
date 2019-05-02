/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that execution doesn't pause twice while stepping, when encountering
 * either a breakpoint or a debugger statement.
 */

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );
      const location = { sourceUrl: source.url, line: debuggee.line0 + 2 };

      threadClient.setBreakpoint(location, {});

      const testCallbacks = [
        function(packet) {
          // Check that the stepping worked.
          Assert.equal(packet.frame.where.line, debuggee.line0 + 5);
          Assert.equal(packet.why.type, "resumeLimit");
        },
        function(packet) {
          // Entered the foo function call frame.
          Assert.equal(packet.frame.where.line, location.line);
          Assert.notEqual(packet.why.type, "breakpoint");
          Assert.equal(packet.why.type, "resumeLimit");
        },
        function(packet) {
          // Check that the breakpoint wasn't the reason for this pause, but
          // that the frame is about to be popped while stepping.
          Assert.equal(packet.frame.where.line, debuggee.line0 + 3);
          Assert.notEqual(packet.why.type, "breakpoint");
          Assert.equal(packet.why.type, "resumeLimit");
          Assert.equal(packet.why.frameFinished.return.type, "undefined");
        },
        function(packet) {
          // Check that the debugger statement wasn't the reason for this pause.
          Assert.equal(debuggee.a, 1);
          Assert.equal(debuggee.b, undefined);
          Assert.equal(packet.frame.where.line, debuggee.line0 + 6);
          Assert.notEqual(packet.why.type, "debuggerStatement");
          Assert.equal(packet.why.type, "resumeLimit");
          Assert.equal(packet.poppedFrames.length, 1);
        },
        function(packet) {
          // Check that the debugger statement wasn't the reason for this pause.
          Assert.equal(packet.frame.where.line, debuggee.line0 + 7);
          Assert.notEqual(packet.why.type, "debuggerStatement");
          Assert.equal(packet.why.type, "resumeLimit");
        },
      ];

      for (const callback of testCallbacks) {
        const waiter = waitForPause(threadClient);
        threadClient.stepIn();
        const packet = await waiter;
        callback(packet);
      }

      // Remove the breakpoint and finish.
      const waiter = waitForPause(threadClient);
      threadClient.stepIn();
      await waiter;
      threadClient.removeBreakpoint(location);

      threadClient.resume().then(resolve);
    });

    /* eslint-disable */
    Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                     "function foo() {\n" + // line0 + 1
                     "  this.a = 1;\n" +    // line0 + 2 <-- Breakpoint is set here.
                     "}\n" +                // line0 + 3
                     "debugger;\n" +        // line0 + 4
                     "foo();\n" +           // line0 + 5
                     "debugger;\n" +        // line0 + 6
                     "var b = 2;\n",        // line0 + 7
                     debuggee);
    /* eslint-enable */
  });
}));
