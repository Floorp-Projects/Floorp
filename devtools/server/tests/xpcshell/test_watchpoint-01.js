/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/*
- Tests adding set and get watchpoints.
- Tests removing a watchpoint.
- Tests removing all watchpoints.
*/

add_task(
  threadFrontTest(async args => {
    await testSetWatchpoint(args);
    await testGetWatchpoint(args);
    await testRemoveWatchpoint(args);
    await testRemoveWatchpoints(args);
  })
);

async function testSetWatchpoint({ commands, threadFront, debuggee }) {
  async function evaluateJS(input) {
    const { result } = await commands.scriptCommand.execute(input, {
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
      "test_watchpoint-01.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we paused on the debugger statement");
  Assert.equal(packet.frame.where.line, 3);

  info("Add set watchpoint");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");

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

async function testGetWatchpoint({ threadFront, debuggee }) {
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
      "test_watchpoint-01.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we paused on the debugger statement");
  Assert.equal(packet.frame.where.line, 3);

  info("Add get watchpoint.");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "get");

  info("Test that watchpoint triggers pause on get.");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "getWatchpoint");
  Assert.equal(obj.preview.ownProperties.a.value, 1);

  await resume(threadFront);
}

async function testRemoveWatchpoint({ threadFront, debuggee }) {
  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a = 2;                        // 4
        debugger;                         // 5
      }                                   //

      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-01.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info(`Test that we paused on the debugger statement`);
  Assert.equal(packet.frame.where.line, 3);

  info(`Add set watchpoint`);
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");

  info(`Remove set watchpoint`);
  await objClient.removeWatchpoint("a");

  info(`Test that we do not pause on set`);
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 5);

  await resume(threadFront);
}

async function testRemoveWatchpoints({ threadFront, debuggee }) {
  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a = 2;                        // 4
        debugger;                         // 5
      }                                   //
      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-01.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we paused on the debugger statement");
  Assert.equal(packet.frame.where.line, 3);

  info("Add and then remove set watchpoint");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");
  await objClient.removeWatchpoints();

  info("Test that we do not pause on set");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 5);

  await resume(threadFront);
}
