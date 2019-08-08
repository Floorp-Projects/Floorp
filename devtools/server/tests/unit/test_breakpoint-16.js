/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that we can set breakpoints in columns, not just lines.
 */

add_task(
  threadFrontTest(async ({ threadFront, client, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );
    const source = await getSourceById(threadFront, packet.frame.where.actor);
    const location = {
      sourceUrl: source.url,
      line: debuggee.line0 + 1,
      column: 55,
    };

    let timesBreakpointHit = 0;
    threadFront.setBreakpoint(location, {});

    while (timesBreakpointHit < 3) {
      await resume(threadFront);
      const packet = await waitForPause(threadFront);
      testAssertions(packet, debuggee, source, location, timesBreakpointHit);

      timesBreakpointHit++;
    }

    threadFront.removeBreakpoint(location);
    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  /* eslint-disable */
      Cu.evalInSandbox(
        "var line0 = Error().lineNumber;\n" +
        "(function () { debugger; this.acc = 0; for (var i = 0; i < 3; i++) this.acc++; }());",
        debuggee
      );
      /* eslint-enable */
}

function testAssertions(
  packet,
  debuggee,
  source,
  location,
  timesBreakpointHit
) {
  Assert.equal(packet.why.type, "breakpoint");
  Assert.equal(packet.frame.where.actor, source.actor);
  Assert.equal(packet.frame.where.line, location.line);
  Assert.equal(packet.frame.where.column, location.column);

  Assert.equal(debuggee.acc, timesBreakpointHit);
  Assert.equal(
    packet.frame.environment.bindings.variables.i.value,
    timesBreakpointHit
  );
}
