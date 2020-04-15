/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/*
- Adds a 'get or set' watchpoint. Tests that the debugger will pause on both get and set.
*/

add_task(
  threadFrontTest(async args => {
    await testGetPauseWithGetOrSetWatchpoint(args);
    await testSetPauseWithGetOrSetWatchpoint(args);
  })
);

async function testGetPauseWithGetOrSetWatchpoint({ threadFront, debuggee }) {
  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a + 4;                        // 4
      }                                   //
      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-05.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we paused on the debugger statement");
  Assert.equal(packet.frame.where.line, 3);

  info("Add get or set watchpoint.");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "getorset");

  info("Test that watchpoint triggers pause on get.");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "getWatchpoint");
  Assert.equal(obj.preview.ownProperties.a.value, 1);

  await resume(threadFront);
}

async function testSetPauseWithGetOrSetWatchpoint({ threadFront, debuggee, targetFront }) {
  async function evaluateJS(input) {
    const consoleFront = await targetFront.getFront("console");
    const { result } = await consoleFront.evaluateJSAsync(input, {
      thread: threadFront.actor,
      frameActor: packet.frame.actorID,
    });
    return result;
  }

  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a = 2;                        // 4
      }                                   //
      stopMe({a: { b: 1 }})`,
      debuggee,
      "1.8",
      "test_watchpoint-05.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we paused on the debugger statement");
  Assert.equal(packet.frame.where.line, 3);

  info("Add get or set watchpoint");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "getorset");

  let result = await evaluateJS("obj.a");
  Assert.equal(result.getGrip().preview.ownProperties.b.value, 1);

  result = await evaluateJS("obj.a.b");
  Assert.equal(result, 1);

  info("Test that watchpoint triggers pause on set");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "setWatchpoint");
  Assert.equal(obj.preview.ownProperties.a.value.ownPropertyLength, 1);

  await resume(threadFront);
}