/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test behavior of blackboxing sources we are currently paused in.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    // Set up
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    threadFront.setBreakpoint({ sourceUrl: BLACK_BOXED_URL, line: 2 }, {});

    // Test black boxing a source while pausing in the source
    const { error, sources } = await threadFront.getSources();
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = threadFront.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    const pausedInSource = await blackBox(sourceFront);
    Assert.ok(
      pausedInSource,
      "We should be notified that we are currently paused in this source"
    );
    await threadFront.resume();
  })
);

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function evalCode(debuggee) {
  /* eslint-disable no-multi-spaces, no-undef */
  // prettier-ignore
  Cu.evalInSandbox(
    "" +
      function doStuff(k) { // line 1
        debugger;           // line 2
        k(100);             // line 3
      },                    // line 4
    debuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );
  // prettier-ignore
  Cu.evalInSandbox(
    "" +
    function runTest() { // line 1
      doStuff(           // line 2
        function(n) {    // line 3
          return n;      // line 4
        }                // line 5
      );                 // line 6
    } +                  // line 7
      "\n runTest();",   // line 8
    debuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-undef */
}
