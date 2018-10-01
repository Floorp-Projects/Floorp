/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that stepping out of a function returns the right return value.
 */

async function invokeAndPause({global, debuggerClient}, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    debuggerClient
  );
}

async function step({threadClient, debuggerClient}, cmd) {
  return cmd(debuggerClient, threadClient);
}

function getPauseLocation(packet) {
  const {line, column} = packet.frame.where;
  return {line, column};
}

function getFrameFinished(packet) {
  return packet.why.frameFinished;
}

async function steps(dbg, sequence) {
  const locations = [];
  for (const cmd of sequence) {
    const packet = await step(dbg, cmd);
    locations.push(getPauseLocation(packet));
  }
  return locations;
}

async function testFinish({threadClient, debuggerClient}) {
  await resume(threadClient);
  await close(debuggerClient);

  do_test_finished();
}

async function testRet(dbg) {
  let packet;

  info(`1. Test returning from doRet via stepping over`);
  await invokeAndPause(dbg, `doRet()`);
  await steps(dbg, [stepOver, stepIn, stepOver]);
  packet = await step(dbg, stepOver);

  deepEqual(
    getPauseLocation(packet),
    {line: 6, column: 0},
    `completion location in doRet`
  );
  deepEqual(
    getFrameFinished(packet),
    {"return": 2}, `completion value`);

  await resume(dbg.threadClient);

  info(`2. Test leaving from doRet via stepping out`);
  await invokeAndPause(dbg, `doRet()`);
  await steps(dbg, [stepOver, stepIn]);

  packet = await step(dbg, stepOut);

  deepEqual(
    getPauseLocation(packet),
    {line: 15, column: 2},
    `completion location in doThrow`
  );

  deepEqual(
    getFrameFinished(packet),
    {"return": 2},
    `completion completion value`
  );

  await resume(dbg.threadClient);
}

async function testThrow(dbg) {
  let packet;

  info(`3. Test leaving from doThrow via stepping over`);
  await invokeAndPause(dbg, `doThrow()`);
  await steps(dbg, [stepOver, stepOver, stepIn]);
  packet = await step(dbg, stepOver);

  deepEqual(
    getPauseLocation(packet),
    {line: 9, column: 8},
    `completion location in doThrow`
  );

  deepEqual(
    getFrameFinished(packet).throw.class,
    "Error",
    `completion value class`
  );
  deepEqual(
    getFrameFinished(packet).throw.preview.message,
    "yo",
    `completion value preview`
  );

  await resume(dbg.threadClient);

  info(`4. Test leaving from doThrow via stepping out`);
  await invokeAndPause(dbg, `doThrow()`);
  await steps(dbg, [stepOver, stepOver, stepIn]);

  packet = await step(dbg, stepOut);
  deepEqual(
    getPauseLocation(packet),
    {line: 22, column: 14},
    `completion location in doThrow`
  );

  deepEqual(
    getFrameFinished(packet).throw.class,
    "Error",
    `completion completion value class`
  );
  deepEqual(
    getFrameFinished(packet).throw.preview.message,
    "yo",
    `completion completion value preview`
  );
  await resume(dbg.threadClient);
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("completions.js");

    await testRet(dbg);
    await testThrow(dbg);

    await testFinish(dbg);
  })();
}
