/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test stepping from inside a blackboxed function
 * test-page: https://dbg-blackbox-stepping2.glitch.me/
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
    invokeAndPause(
      dbg,
      `function outermost() {
        blackboxed(
          function inner1() {
            return 1;
          },
          function inner2() {
            return 2;
          }
        );
      }`,
      "http://example.com/unblackboxed.js"
    );
    invokeAndPause(
      dbg,
      `function blackboxed(...args) {
        for (const arg of args) {
          arg();
        }
      }`,
      "http://example.com/blackboxed.js"
    );

    const { sources } = await getSources(threadFront);
    const blackboxedSourceFront = threadFront.source(
      sources.find(source => source.url == "http://example.com/blackboxed.js")
    );
    const unblackboxedSource = sources.find(
      source => source.url == "http://example.com/unblackboxed.js"
    );
    const unblackboxedActor = unblackboxedSource.actor;
    const unblackboxedSourceFront = threadFront.source(unblackboxedSource);

    await setBreakpoint(threadFront, {
      sourceUrl: unblackboxedSourceFront.url,
      line: 4,
    });
    blackBox(blackboxedSourceFront);

    async function testStepping(action, expectedLine) {
      commands.scriptCommand.execute("outermost()");
      await waitForPause(threadFront);
      await stepOver(threadFront);
      const packet = await action(threadFront);
      const { actor, line } = packet.frame.where;
      equal(actor, unblackboxedActor, "Paused in unblackboxed source");
      equal(line, expectedLine, "Paused at correct line");
      await threadFront.resume();
    }

    info("Step Out to outermost");
    await testStepping(stepOut, 10);

    info("Step Over to outermost");
    await testStepping(stepOver, 10);

    info("Step In to inner2");
    await testStepping(stepIn, 7);
  })
);
