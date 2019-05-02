/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint on a line without code will skip
 * forward when we know the script isn't GCed (the debugger is connected,
 * so it's kept alive).
 */

var test_no_skip_breakpoint = async function(source, location, debuggee) {
  const [response, bpClient] = await source.setBreakpoint(
    Object.assign({}, location, { noSliding: true })
  );

  Assert.ok(!response.actualLocation);
  Assert.equal(bpClient.location.line, debuggee.line0 + 3);
  await bpClient.remove();
};

add_task(threadClientTest(({ threadClient, debuggee }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", async function(event, packet) {
      const location = { line: debuggee.line0 + 3 };
      const source = await getSourceById(
        threadClient,
        packet.frame.where.actor
      );
      // First, make sure that we can disable sliding with the
      // `noSliding` option.
      await test_no_skip_breakpoint(source, location, debuggee);

      // Now make sure that the breakpoint properly slides forward one line.
      const [response, bpClient] = await source.setBreakpoint(location);
      Assert.ok(!!response.actualLocation);
      Assert.equal(response.actualLocation.source.actor, source.actor);
      Assert.equal(response.actualLocation.line, location.line + 1);

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

        // Remove the breakpoint.
        bpClient.remove(function(response) {
          threadClient.resume().then(resolve);
        });
      });

      threadClient.resume();
    });

    // Use `evalInSandbox` to make the debugger treat it as normal
    // globally-scoped code, where breakpoint sliding rules apply.
    /* eslint-disable */
    Cu.evalInSandbox(
      "var line0 = Error().lineNumber;\n" +
      "debugger;\n" +      // line0 + 1
      "var a = 1;\n" +     // line0 + 2
      "// A comment.\n" +  // line0 + 3
      "var b = 2;",        // line0 + 4
      debuggee
    );
    /* eslint-enable */
  });
}));
