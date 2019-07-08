/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check scenarios where we're leaving function a and
 * going to the function b's call-site.
 */

async function testFinish({ threadClient, debuggerClient }) {
  await close(debuggerClient);

  do_test_finished();
}

async function invokeAndPause({ global, threadClient }, expression) {
  return executeOnNextTickAndWaitForPause(
    () => Cu.evalInSandbox(expression, global),
    threadClient
  );
}

async function step(threadClient, cmd) {
  return cmd(threadClient);
}

function getPauseLocation(packet) {
  const { line, column } = packet.frame.where;
  return { line, column };
}

function getPauseReturn(packet) {
  dump(`>> getPauseReturn yo ${JSON.stringify(packet.why)}\n`);
  return packet.why.frameFinished.return;
}

async function steps(threadClient, sequence) {
  const locations = [];
  for (const cmd of sequence) {
    const packet = await step(threadClient, cmd);
    locations.push(getPauseLocation(packet));
  }
  return locations;
}

async function stepOutOfA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  const { threadClient } = dbg;
  await steps(threadClient, [stepOver, stepIn]);

  dump(`>>> oof\n`);
  const packet = await stepOut(threadClient);
  dump(`>>> foo\n`);

  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );

  await threadClient.resume();
}

async function stepOverInA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  const { threadClient } = dbg;
  await steps(threadClient, [stepOver, stepIn]);

  let packet = await stepOver(threadClient);
  dump(`>> stepOverInA hi\n`);
  equal(getPauseReturn(packet).ownPropertyLength, 1, "a() is returning obj");

  packet = await stepOver(threadClient);
  deepEqual(
    getPauseLocation(packet),
    expectedLocation,
    `step out location in ${func}`
  );
  await dbg.threadClient.resume();
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
