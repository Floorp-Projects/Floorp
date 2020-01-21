/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we only break on offsets that are entry points for the line we are
 * breaking on. Bug 907278.
 */

add_task(
  threadFrontTest(async ({ threadFront, client, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = { sourceUrl: source.url, line: 3 };
    threadFront.setBreakpoint(location, {});
    await client.waitForRequestsToSettle();

    debuggee.console = { log: x => void x };

    await resume(threadFront);

    const packet2 = await executeOnNextTickAndWaitForPause(
      debuggee.test,
      threadFront
    );
    Assert.equal(packet2.why.type, "breakpoint");

    threadFront.resume();

    const packet3 = await waitForPause(threadFront);
    testDbgStatement(packet3);

    await resume(threadFront);
  })
);

function evaluateTestCode(debuggee) {
  Cu.evalInSandbox(
    "debugger;\n" +
      function test() {
        console.log("foo bar");
        debugger;
      },
    debuggee,
    "1.8",
    "http://example.com/",
    1
  );
}

function testDbgStatement({ why }) {
  // Should continue to the debugger statement.
  Assert.equal(why.type, "debuggerStatement");
  // Not break on another offset from the same line (that isn't an entry point
  // to the line)
  Assert.notEqual(why.type, "breakpoint");
}
