/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1333219 - make that setBreakpoint fails when script is not found
 * at the specified line.
 */

add_task(
  threadClientTest(async ({ threadClient, debuggee }) => {
    // Populate the `ScriptStore` so that we only test that the script
    // is added through `onNewScript`
    await getSources(threadClient);

    const packet = await executeOnNextTickAndWaitForPause(() => {
      evalCode(debuggee);
    }, threadClient);
    const source = await getSourceById(threadClient, packet.frame.where.actor);

    const location = {
      line: debuggee.line0 + 2,
    };

    const [res] = await setBreakpoint(source, location);
    ok(!res.error);

    const location2 = {
      line: debuggee.line0 + 7,
    };

    await source.setBreakpoint(location2).then(
      () => {
        do_throw("no code shall not be found the specified line or below it");
      },
      reason => {
        Assert.equal(reason.error, "noCodeAtLineColumn");
        ok(reason.message);
      }
    );

    await resume(threadClient);
  })
);

function evalCode(debuggee) {
  // Start a new script
  Cu.evalInSandbox(
    `
var line0 = Error().lineNumber;
function some_function() {
  // breakpoint is valid here -- it slides one line below (line0 + 2)
}
debugger;
// no breakpoint is allowed after the EOF (line0 + 6)
`,
    debuggee
  );
}
