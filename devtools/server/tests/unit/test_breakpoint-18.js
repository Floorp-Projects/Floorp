/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we only break on offsets that are entry points for the line we are
 * breaking on. Bug 907278.
 */

add_task(
  threadClientTest(({ threadClient, debuggee, client }) => {
    return new Promise(resolve => {
      // Expose console as the test script uses it
      debuggee.console = { log: x => void x };

      // Inline all paused listeners as promises won't resolve when paused
      threadClient.once("paused", async packet1 => {
        await setBreakpoint(packet1, threadClient, client);

        threadClient.once("paused", ({ why }) => {
          Assert.equal(why.type, "breakpoint");

          threadClient.once("paused", packet3 => {
            testDbgStatement(packet3);
            resolve();
          });
          threadClient.resume();
        });
        debuggee.test();
      });

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
    });
  })
);

function setBreakpoint(packet, threadClient, client) {
  return new Promise(async resolve => {
    const source = await getSourceById(threadClient, packet.frame.where.actor);
    threadClient.once("resumed", resolve);

    threadClient.setBreakpoint({ sourceUrl: source.url, line: 3 }, {});
    await client.waitForRequestsToSettle();
    await threadClient.resume();
  });
}

function testDbgStatement({ why }) {
  // Should continue to the debugger statement.
  Assert.equal(why.type, "debuggerStatement");
  // Not break on another offset from the same line (that isn't an entry point
  // to the line)
  Assert.notEqual(why.type, "breakpoint");
}
