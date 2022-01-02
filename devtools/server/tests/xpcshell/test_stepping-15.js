/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test stepping from inside a blackboxed function
 * test-page: https://dbg-blackbox-stepping.glitch.me/
 */

async function invokeAndPause({ global, threadFront }, expression, url) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global, "1.8", url, 1),
    threadFront
  );
}
add_task(
  threadFrontTest(async ({ commands, threadFront, debuggee }) => {
    const dbg = { global: debuggee, threadFront };

    // Test stepping from a blackboxed location
    async function testStepping(action, expectedLine) {
      commands.scriptCommand.execute(`outermost()`);
      await waitForPause(threadFront);
      await blackBox(blackboxedSourceFront);
      const packet = await action(threadFront);
      const { line, actor } = packet.frame.where;
      equal(actor, unblackboxedActor, "paused in unblackboxed source");
      equal(line, expectedLine, "paused at correct line");
      await threadFront.resume();
      await unBlackBox(blackboxedSourceFront);
    }

    invokeAndPause(
      dbg,
      `function outermost() {
        const value = blackboxed1();
        return value + 1;
      }
      function innermost() {
        return 1;
      }`,
      "http://example.com/unblackboxed.js"
    );
    invokeAndPause(
      dbg,
      `function blackboxed1() {
        return blackboxed2();
      }
      function blackboxed2() {
        return innermost();
      }`,
      "http://example.com/blackboxed.js"
    );

    const { sources } = await getSources(threadFront);
    const blackboxedSourceFront = threadFront.source(
      sources.find(source => source.url == "http://example.com/blackboxed.js")
    );
    const unblackboxedActor = sources.find(
      source => source.url == "http://example.com/unblackboxed.js"
    ).actor;

    await setBreakpoint(threadFront, {
      sourceUrl: blackboxedSourceFront.url,
      line: 5,
    });

    info("Step Out to outermost");
    await testStepping(stepOut, 3);

    info("Step Over to outermost");
    await testStepping(stepOver, 3);

    info("Step In to innermost");
    await testStepping(stepIn, 6);
  })
);
