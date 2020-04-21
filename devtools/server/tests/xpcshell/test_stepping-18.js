/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check scenarios where we're leaving function a and
 * going to the function b's call-site.
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

async function stepOutOfA(dbg, func, frameIndex, expectedLocation) {
  const { threadFront } = dbg;

  info("pause and step into a()");
  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn, stepIn]);

  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[frameIndex].actorID;
  const packet = await stepOut(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step over location in ${func}`
  );

  await dbg.threadFront.resume();
}

async function stepOverInA(dbg, func, frameIndex, expectedLocation) {
  const { threadFront } = dbg;

  info("pause and step into a()");
  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn]);

  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[frameIndex].actorID;
  const packet = await stepOver(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step over location in ${func}`
  );

  await dbg.threadFront.resume();
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("stepping.js");

    info(`Test step over with the 1st frame`);
    await stepOverInA(dbg, "arithmetic", 0, { line: 8, column: 0 });

    info(`Test step over with the 2nd frame`);
    await stepOverInA(dbg, "arithmetic", 1, { line: 17, column: 0 });

    info(`Test step out with the 1st frame`);
    await stepOutOfA(dbg, "nested", 0, { line: 31, column: 0 });

    info(`Test step out with the 2nd frame`);
    await stepOutOfA(dbg, "nested", 1, { line: 36, column: 0 });

    await testFinish(dbg);
  })();
}
