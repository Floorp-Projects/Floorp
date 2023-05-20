/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that step out stops at the async parent's frame.
 */

async function testFinish({ threadFront, devToolsClient }) {
  await close(devToolsClient);

  do_test_finished();
}

async function invokeAndPause({ global, threadFront }, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    threadFront
  );
}

async function steps(threadFront, sequence) {
  const locations = [];
  for (const cmd of sequence) {
    const packet = await step(threadFront, cmd);
    locations.push(getPauseLocation(packet));
  }
  return locations;
}

async function step(threadFront, cmd) {
  return cmd(threadFront);
}

function getPauseLocation(packet) {
  const { line, column } = packet.frame.where;
  return { line, column };
}

async function stepOutBeforeTimer(dbg, func, frameIndex, expectedLocation) {
  const { threadFront } = dbg;

  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn]);

  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[frameIndex].actorID;
  const packet = await stepOut(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );

  await resumeAndWaitForPause(threadFront);
  await resume(threadFront);
}

async function stepOutAfterTimer(dbg, func, frameIndex, expectedLocation) {
  const { threadFront } = dbg;

  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn, stepOver, stepOver]);

  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[frameIndex].actorID;
  const packet = await stepOut(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );

  await resumeAndWaitForPause(threadFront);
  await dbg.threadFront.resume();
}

function run_test() {
  return (async function () {
    const dbg = await setupTestFromUrl("stepping-async.js");

    info(`Test stepping out before timer;`);
    await stepOutBeforeTimer(dbg, "stuff", 0, { line: 27, column: 2 });

    info(`Test stepping out after timer;`);
    await stepOutAfterTimer(dbg, "stuff", 0, { line: 29, column: 2 });

    await testFinish(dbg);
  })();
}
