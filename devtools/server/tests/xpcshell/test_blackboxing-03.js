/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't stop at debugger statements inside black boxed sources.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Set up
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const source = await getSourceById(threadFront, packet.frame.where.actor);
    threadFront.setBreakpoint({ sourceUrl: source.url, line: 4 }, {});
    await threadFront.resume();

    // Test the debugger statement in the black boxed source
    await threadFront.getSources();
    const sourceFront = await getSource(threadFront, BLACK_BOXED_URL);

    await blackBox(sourceFront);

    const packet2 = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet2.why.type,
      "breakpoint",
      "We should pass over the debugger statement."
    );

    threadFront.removeBreakpoint({ sourceUrl: source.url, line: 4 }, {});

    await threadFront.resume();

    // Test the debugger statement in the unblack boxed source
    await unBlackBox(sourceFront);

    const packet3 = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet3.why.type,
      "debuggerStatement",
      "We should stop at the debugger statement again"
    );
    await threadFront.resume();

    // Test the debugger statement in the black boxed range
    threadFront.setBreakpoint({ sourceUrl: source.url, line: 4 }, {});

    await blackBox(sourceFront, {
      start: { line: 1, column: 0 },
      end: { line: 9, column: 0 },
    });

    const packet4 = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet4.why.type,
      "breakpoint",
      "We should pass over the debugger statement."
    );

    threadFront.removeBreakpoint({ sourceUrl: source.url, line: 4 }, {});
    await unBlackBox(sourceFront);
    await threadFront.resume();
  })
);

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function evalCode(debuggee) {
  /* eslint-disable no-multi-spaces, no-undef */
  // prettier-ignore
  Cu.evalInSandbox(
    "" + function doStuff(k) { // line 1
      debugger;                // line 2 - Break here
      k(100);                  // line 3
    },                         // line 4
    debuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );
  // prettier-ignore
  Cu.evalInSandbox(
    "" + function runTest() { // line 1
      doStuff(                // line 2
        function(n) {         // line 3
          Math.abs(n);        // line 4 - Break here
        }                     // line 5
      );                      // line 6
    }                         // line 7
    + "\n debugger;",         // line 8
    debuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-undef */
}
