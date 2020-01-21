/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1122064 - make sure that scripts introduced via onNewScripts
 * properly populate the `ScriptStore` with all there nested child
 * scripts, so you can set breakpoints on deeply nested scripts
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Populate the `ScriptStore` so that we only test that the script
    // is added through `onNewScript`
    await getSources(threadFront);

    let packet = await executeOnNextTickAndWaitForPause(() => {
      evalCode(debuggee);
    }, threadFront);
    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = {
      sourceUrl: source.url,
      line: debuggee.line0 + 8,
    };

    setBreakpoint(threadFront, location);

    await resume(threadFront);
    packet = await waitForPause(threadFront);
    Assert.equal(packet.why.type, "breakpoint");
    Assert.equal(packet.frame.where.actor, source.actor);
    Assert.equal(packet.frame.where.line, location.line);

    await resume(threadFront);
  })
);

/* eslint-disable */
function evalCode(debuggee) {
  // Start a new script
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n(" + function () {
      debugger;
      var a = (function () {
        return (function () {
          return (function () {
            return (function () {
              return (function () {
                var x = 10; // This line gets a breakpoint
                return 1;
              })();
            })();
          })();
        })();
      })();
    } + ")()",
    debuggee
  );
}
