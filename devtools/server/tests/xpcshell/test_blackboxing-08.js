/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test blackbox ranges
 */

async function testFinish({ threadFront, debuggerClient }) {
  await threadFront.resume();
  await close(debuggerClient);

  do_test_finished();
}

async function invokeAndPause({ global, threadFront }, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    threadFront
  );
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("stepping.js");
    const { threadFront } = dbg;

    await invokeAndPause(dbg, `chaining()`);

    const { sources } = await getSources(threadFront);
    const sourceFront = threadFront.source(sources[0]);

    await setBreakpoint(threadFront, { sourceUrl: sourceFront.url, line: 7 });
    await setBreakpoint(threadFront, { sourceUrl: sourceFront.url, line: 11 });

    // 1. lets blackbox function a, and assert that we pause in b
    const range = { start: { line: 6, column: 0 }, end: { line: 8, colum: 1 } };
    blackBox(sourceFront, range);
    await threadFront.resume();
    const paused = await waitForPause(threadFront);
    equal(paused.frame.where.line, 11, "paused inside of b");
    await threadFront.resume();

    // 2. lets unblackbox function a, and assert that we pause in a
    unBlackBox(sourceFront, range);
    await invokeAndPause(dbg, `chaining()`);
    await threadFront.resume();
    const paused2 = await waitForPause(threadFront);
    equal(paused2.frame.where.line, 7, "paused inside of a");

    await testFinish(dbg);
  })();
}
