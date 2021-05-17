/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test exceptions inside black boxed sources.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, commands }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const { error } = await threadFront.getSources();
    Assert.ok(!error, "Should not get an error: " + error);

    const sourceFront = await getSource(threadFront, BLACK_BOXED_URL);
    await blackBox(sourceFront);
    await commands.threadConfigurationCommand.updateConfiguration({
      pauseOnExceptions: true,
      ignoreCaughtExceptions: false,
    });

    threadFront.resume();
    const packet = await waitForPause(threadFront);
    const source = await getSourceById(threadFront, packet.frame.where.actor);

    Assert.equal(
      source.url,
      SOURCE_URL,
      "We shouldn't pause while in the black boxed source."
    );

    await unBlackBox(sourceFront);
    await blackBox(sourceFront, {
      start: { line: 1, column: 0 },
      end: { line: 4, column: 0 },
    });

    await threadFront.resume();

    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    threadFront.resume();
    const packet2 = await waitForPause(threadFront);
    const source2 = await getSourceById(threadFront, packet2.frame.where.actor);

    Assert.equal(
      source2.url,
      SOURCE_URL,
      "We shouldn't pause while in the black boxed source."
    );

    await threadFront.resume();
  })
);

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function evalCode(debuggee) {
  /* eslint-disable no-multi-spaces, no-unreachable, no-undef */
  // prettier-ignore
  Cu.evalInSandbox(
    "" +
      function doStuff(k) {           // line 1
        throw new Error("error msg"); // line 2
        k(100);                       // line 3
      },                              // line 4
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
          debugger;      // line 4
        }                // line 5
      );                 // line 6
    } +                  // line 7
    "\ndebugger;\n" +    // line 8
      "try { runTest() } catch (ex) { }", // line 9
    debuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-unreachable, no-undef */
}
