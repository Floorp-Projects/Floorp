/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test blackbox ranges
 */

async function testFinish({ threadClient, debuggerClient }) {
  await threadClient.resume();
  await close(debuggerClient);

  do_test_finished();
}

async function invokeAndPause({ global, threadClient }, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    threadClient
  );
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("stepping.js");
    const { threadClient } = dbg;

    await invokeAndPause(dbg, `chaining()`);

    const { sources } = await getSources(threadClient);
    const sourceFront = threadClient.source(sources[0]);

    await setBreakpoint(threadClient, { sourceUrl: sourceFront.url, line: 7 });
    await setBreakpoint(threadClient, { sourceUrl: sourceFront.url, line: 11 });

    // 1. lets blackbox function a, and assert that we pause in b
    const range = { start: { line: 6, column: 0 }, end: { line: 8, colum: 1 } };
    blackBox(sourceFront, range);
    await threadClient.resume();
    const paused = await waitForPause(threadClient);
    equal(paused.frame.where.line, 11, "paused inside of b");
    await threadClient.resume();

    // 2. lets unblackbox function a, and assert that we pause in a
    unBlackBox(sourceFront, range);
    await invokeAndPause(dbg, `chaining()`);
    await threadClient.resume();
    const paused2 = await waitForPause(threadClient);
    equal(paused2.frame.where.line, 7, "paused inside of a");

    await testFinish(dbg);
  })();
}
