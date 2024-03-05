/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check restarting a frame and stepping out of the
 * restarted frame.
 */

async function testFinish({ devToolsClient }) {
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

async function restartFrame0(dbg, func, expectedLocation) {
  const { threadFront } = dbg;

  info("pause and step into a()");
  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn]);

  info("restart the youngest frame a()");
  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[0].actorID;
  const packet = await restartFrame(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    "pause location in the restarted frame a()"
  );
}

async function restartFrame1(dbg, func, expectedLocation) {
  const { threadFront } = dbg;

  info("pause and step into b()");
  await invokeAndPause(dbg, `${func}()`);
  await steps(threadFront, [stepOver, stepIn, stepIn]);

  info("restart the frame with index 1");
  const { frames } = await threadFront.frames(0, 5);
  const frameActorID = frames[1].actorID;
  const packet = await restartFrame(threadFront, frameActorID);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    "pause location in the restarted frame c()"
  );
}

async function stepOutRestartedFrame(
  dbg,
  restartedFrameName,
  expectedLocation,
  expectedCallstackLength
) {
  const { threadFront } = dbg;
  const { frames } = await threadFront.frames(0, 5);

  Assert.equal(
    frames.length,
    expectedCallstackLength,
    `the callstack length after restarting frame ${restartedFrameName}()`
  );

  info(`step out of the restarted frame ${restartedFrameName}()`);
  const frameActorID = frames[0].actorID;
  const packet = await stepOut(threadFront, frameActorID);

  deepEqual(getPauseLocation(packet), expectedLocation, `step out location`);
}

function run_test() {
  return (async function () {
    const dbg = await setupTestFromUrl("stepping.js");

    info(`Test restarting the youngest frame`);
    await restartFrame0(dbg, "arithmetic", { line: 7, column: 2 });
    await stepOutRestartedFrame(dbg, "a", { line: 16, column: 8 }, 3);
    await dbg.threadFront.resume();

    info(`Test restarting the frame with the index 1`);
    await restartFrame1(dbg, "nested", { line: 30, column: 2 });
    await stepOutRestartedFrame(dbg, "c", { line: 36, column: 0 }, 3);
    await dbg.threadFront.resume();

    await testFinish(dbg);
  })();
}
