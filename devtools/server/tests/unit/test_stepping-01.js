/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check scenarios where we're leaving function a and
 * going to the function b's call-site.
 */

async function testFinish({threadClient, debuggerClient}) {
  await resume(threadClient);
  await close(debuggerClient);

  do_test_finished();
}

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

function getPauseReturn(packet) {
  dump(`>> getPauseReturn yo ${JSON.stringify(packet.why)}\n`);
  return packet.why.frameFinished.return;
}

async function steps(dbg, sequence) {
  const locations = [];
  for (const cmd of sequence) {
    const packet = await step(dbg, cmd);
    locations.push(getPauseLocation(packet));
  }
  return locations;
}

async function stepOutOfA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  await steps(dbg, [stepOver, stepIn]);

  dump(`>>> oof\n`);
  const packet = await step(dbg, stepOut);
  dump(`>>> foo\n`);

  deepEqual(getPauseLocation(packet), expectedLocation, `step out location in ${func}`);

  await resume(dbg.threadClient);
}

async function stepOverInA(dbg, func, expectedLocation) {
  await invokeAndPause(dbg, `${func}()`);
  await steps(dbg, [stepOver, stepIn, stepOver]);

  let packet = await step(dbg, stepOver);
  dump(`>> stepOverInA hi\n`);
  equal(getPauseReturn(packet).ownPropertyLength, 1, "a() is returning obj");

  packet = await step(dbg, stepOver);
  deepEqual(getPauseLocation(packet), expectedLocation, `step out location in ${func}`);

  await resume(dbg.threadClient);
}

async function testStep(dbg, func, expectedLocation) {
  await stepOverInA(dbg, func, expectedLocation);
  await stepOutOfA(dbg, func, expectedLocation);
}

function run_test() {
  return (async function() {
    const dbg = await setupTestFromUrl("stepping.js");

    await testStep(dbg, "arithmetic", {line: 16, column: 8});
    await testStep(dbg, "composition", {line: 21, column: 2});
    await testStep(dbg, "chaining", {line: 26, column: 6});

    await testFinish(dbg);
  })();
}
