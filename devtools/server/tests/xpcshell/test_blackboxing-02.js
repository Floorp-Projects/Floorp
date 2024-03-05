/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't hit breakpoints in black boxed sources, and that when we
 * unblack box the source again, the breakpoint hasn't disappeared and we will
 * hit it again.
 */

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Set up
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    threadFront.setBreakpoint({ sourceUrl: BLACK_BOXED_URL, line: 2 }, {});
    await threadFront.resume();

    // Test the breakpoint in the black boxed source
    const { error, sources } = await threadFront.getSources();
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = threadFront.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    await blackBox(sourceFront);

    const packet1 = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet1.why.type,
      "debuggerStatement",
      "We should pass over the breakpoint since the source is black boxed."
    );

    await threadFront.resume();

    // Test the breakpoint in the unblack boxed source
    await unBlackBox(sourceFront);

    const packet2 = await executeOnNextTickAndWaitForPause(
      debuggee.runTest,
      threadFront
    );

    Assert.equal(
      packet2.why.type,
      "breakpoint",
      "We should hit the breakpoint again"
    );

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable no-undef */
  // prettier-ignore
  Cu.evalInSandbox(
    "" + function doStuff(k) { // line 1
      const arg = 15;          // line 2 - Break here
      k(arg);                  // line 3
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
        function() {         // line 3
          debugger;           // line 5
        }                     // line 6
      );                      // line 7
    }                         // line 8
    + "\n debugger;",         // line 9
    debuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-undef */
}
