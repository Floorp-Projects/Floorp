/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line without code in a deeply-nested
 * child script will skip forward.
 */

add_task(
  threadFrontTest(({ threadFront, debuggee }) => {
    return new Promise(resolve => {
      threadFront.once("paused", async function(packet) {
        const source = await getSourceById(
          threadFront,
          packet.frame.where.actor
        );
        const location = { line: debuggee.line0 + 5 };

        source.setBreakpoint(location).then(function([response, bpClient]) {
          // Check that the breakpoint has properly skipped forward one line.
          Assert.equal(response.actualLocation.source.actor, source.actor);
          Assert.equal(response.actualLocation.line, location.line + 1);

          threadFront.once("paused", function(packet) {
            // Check the return value.
            Assert.equal(packet.frame.where.actor, source.actor);
            Assert.equal(packet.frame.where.line, location.line + 1);
            Assert.equal(packet.why.type, "breakpoint");
            Assert.equal(packet.why.actors[0], bpClient.actor);
            // Check that the breakpoint worked.
            Assert.equal(debuggee.a, 1);
            Assert.equal(debuggee.b, undefined);

            // Remove the breakpoint.
            bpClient.remove(function(response) {
              threadFront.resume().then(resolve);
            });
          });

          // Continue until the breakpoint is hit.
          threadFront.resume();
        });
      });

      /* eslint-disable */
      Cu.evalInSandbox(
        "var line0 = Error().lineNumber;\n" +
        "function foo() {\n" +     // line0 + 1
        "  function bar() {\n" +   // line0 + 2
        "    function baz() {\n" + // line0 + 3
        "      this.a = 1;\n" +    // line0 + 4
        "      // A comment.\n" +  // line0 + 5
        "      this.b = 2;\n" +    // line0 + 6
        "    }\n" +                // line0 + 7
        "    baz();\n" +           // line0 + 8
        "  }\n" +                  // line0 + 9
        "  bar();\n" +             // line0 + 10
        "}\n" +                    // line0 + 11
        "debugger;\n" +            // line0 + 12
        "foo();\n",               // line0 + 13
        debuggee
      );
      /* eslint-enable */
    });
  })
);
