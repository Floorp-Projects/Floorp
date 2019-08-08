/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check scenarios where we're leaving function a and
 * going to the function b's call-site.
 */

async function testFinish({ threadFront, debuggerClient }) {
  await close(debuggerClient);

  do_test_finished();
}

async function invokeAndPause({ global, threadFront }, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    threadFront
  );
}

async function step(threadFront, cmd) {
  return cmd(threadFront);
}

function getPauseLocation(packet) {
  const { line, column } = packet.frame.where;
  return { line, column };
}

function getPauseReturn(packet) {
  dump(`>> getPauseReturn yo ${JSON.stringify(packet.why)}\n`);
  return packet.why.frameFinished.return;
}

async function steps(threadFront, sequence) {
  const locations = [];
  for (const cmd of sequence) {
    const packet = await step(threadFront, cmd);
    locations.push(getPauseLocation(packet));
  }
  return locations;
}

async function stepOutOfA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  const { threadFront } = dbg;
  await steps(threadFront, [stepOver, stepIn]);

  dump(`>>> oof\n`);
  const packet = await stepOut(threadFront);
  dump(`>>> foo\n`);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );

  await threadFront.resume();
}

async function stepOverInA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  const { threadFront } = dbg;
  await steps(threadFront, [stepOver, stepIn]);

  let packet = await stepOver(threadFront);
  dump(`>> stepOverInA hi\n`);
  equal(getPauseReturn(packet).ownPropertyLength, 1, "a() is returning obj");

  packet = await stepOver(threadFront);
  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );
  await dbg.threadFront.resume();
}

async function testStep(dbg, func, expectedLocation) {
  await stepOverInA(dbg, func, expectedLocation);
  await stepOutOfA(dbg, func, expectedLocation);
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("stepping.js");

    await testStep(dbg, "arithmetic", { line: 17, column: 0 });
    await testStep(dbg, "composition", { line: 22, column: 0 });
    await testStep(dbg, "chaining", { line: 27, column: 0 });

    await testFinish(dbg);
  })();
}
