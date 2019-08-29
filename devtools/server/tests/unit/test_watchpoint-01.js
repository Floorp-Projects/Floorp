/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/*
Tests adding set and get watchpoints.
Tests removing a watchpoint.
*/

add_task(
  threadFrontTest(async args => {
    await testSetWatchpoint(args);
    await testGetWatchpoint(args);
    await testRemoveWatchpoint(args);
  })
);

async function testSetWatchpoint({ threadFront, debuggee }) {
  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a = 2;                        // 4
      }                                   // 
      stopMe({a: 1})`,                           
      debuggee,
      "1.8",
      "test_watchpoint-01.js",
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  //Test that we paused on the debugger statement.
  Assert.equal(packet.frame.where.line, 3);

  //Add set watchpoint.
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");

  //Test that watchpoint triggers pause on set.
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "watchpoint");
  Assert.equal(obj.preview.ownProperties.a.value, 1);
  
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
      "test_watchpoint-01.js",
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  //Test that we paused on the debugger statement.
  Assert.equal(packet.frame.where.line, 3);

  //Add get watchpoint.
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "get");

  //Test that watchpoint triggers pause on get.
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "watchpoint");
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
      "test_watchpoint-01.js",
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );
  
  //Test that we paused on the debugger statement.
  Assert.equal(packet.frame.where.line, 3);

  //Add and then remove set watchpoint.
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");
  await objClient.removeWatchpoint("a");

  //Test that we do not pause on set. 
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 5);
  
  await resume(threadFront);
}