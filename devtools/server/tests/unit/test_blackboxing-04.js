/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test behavior of blackboxing sources we are currently paused in.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_black_box();
    },
    { waitForFinish: true }
  )
);

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function test_black_box() {
  gThreadFront.once("paused", function(packet) {
    gThreadFront.setBreakpoint({ sourceUrl: BLACK_BOXED_URL, line: 2 }, {});
    test_black_box_paused();
  });

  /* eslint-disable no-multi-spaces, no-undef */
  Cu.evalInSandbox(
    "" +
      function doStuff(k) {
        // line 1
        debugger; // line 2
        k(100); // line 3
      }, // line 4
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Cu.evalInSandbox(
    "" +
    function runTest() {
      // line 1
      doStuff(
        // line 2
        function(n) {
          // line 3
          return n; // line 4
        } // line 5
      ); // line 6
    } + // line 7
      "\n runTest();", // line 8
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-undef */
}

function test_black_box_paused() {
  gThreadFront.getSources().then(async function({ error, sources }) {
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = gThreadFront.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    const pausedInSource = await blackBox(sourceFront);
    Assert.ok(
      pausedInSource,
      "We should be notified that we are currently paused in this source"
    );
    await gThreadFront.resume();
    threadFrontTestFinished();
  });
}
