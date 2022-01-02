/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/*
Test that debugger advances instead of pausing twice on the
same line when encountering both a watchpoint and a breakpoint.
*/

add_task(
  threadFrontTest(async args => {
    await testBreakpointAndSetWatchpoint(args);
    await testBreakpointAndGetWatchpoint(args);
    await testLoops(args);
  })
);

// Test that we advance to the next line when a location
// has both a breakpoint and set watchpoint.
async function testBreakpointAndSetWatchpoint({
  commands,
  threadFront,
  debuggee,
}) {
  async function evaluateJS(input) {
    const { result } = await commands.scriptCommand.execute(input, {
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
        debugger;                         // 5
      }                                   //
      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-02.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we pause on the debugger statement.");
  Assert.equal(packet.frame.where.line, 3);
  Assert.equal(packet.why.type, "debuggerStatement");

  info("Add set watchpoint.");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");

  info("Add breakpoint.");
  const source = await getSourceById(threadFront, packet.frame.where.actor);

  const location = {
    sourceUrl: source.url,
    line: 4,
  };

  threadFront.setBreakpoint(location, {});

  info("Test that pause occurs on breakpoint.");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "breakpoint");

  const packet3 = await resumeAndWaitForPause(threadFront);

  info("Test that we pause on the second debugger statement.");
  Assert.equal(packet3.frame.where.line, 5);
  Assert.equal(packet3.why.type, "debuggerStatement");

  info("Test that the value has updated.");
  const result = await evaluateJS("obj.a");
  Assert.equal(result, 2);

  info("Remove breakpoint and finish.");
  threadFront.removeBreakpoint(location, {});

  await resume(threadFront);
}

// Test that we advance to the next line when a location
// has both a breakpoint and get watchpoint.
async function testBreakpointAndGetWatchpoint({ threadFront, debuggee }) {
  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        debugger;                         // 3
        obj.a + 4;                        // 4
        debugger;                         // 5
      }                                   //
      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-02.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we pause on the debugger statement.");
  Assert.equal(packet.frame.where.line, 3);

  info("Add get watchpoint.");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "get");

  info("Add breakpoint.");
  const source = await getSourceById(threadFront, packet.frame.where.actor);

  const location = {
    sourceUrl: source.url,
    line: 4,
  };

  threadFront.setBreakpoint(location, {});

  info("Test that pause occurs on breakpoint.");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 4);
  Assert.equal(packet2.why.type, "breakpoint");

  const packet3 = await resumeAndWaitForPause(threadFront);

  info("Test that we pause on the second debugger statement.");
  Assert.equal(packet3.frame.where.line, 5);
  Assert.equal(packet3.why.type, "debuggerStatement");

  info("Remove breakpoint and finish.");
  threadFront.removeBreakpoint(location, {});

  await resume(threadFront);
}

// Test that we can pause multiple times
// on the same line for a watchpoint.
async function testLoops({ commands, threadFront, debuggee }) {
  async function evaluateJS(input) {
    const { result } = await commands.scriptCommand.execute(input, {
      frameActor: packet.frame.actorID,
    });
    return result;
  }

  function evaluateTestCode(debuggee) {
    /* eslint-disable */
    Cu.evalInSandbox(
      `                                   // 1
      function stopMe(obj) {              // 2
        let i = 0;                        // 3
        debugger;                         // 4
        while (i++ < 2) {                 // 5
          obj.a = 2;                      // 6
        }                                 // 7
        debugger;                         // 8
      }                                   //
      stopMe({a: 1})`,
      debuggee,
      "1.8",
      "test_watchpoint-02.js"
    );
    /* eslint-disable */
  }

  const packet = await executeOnNextTickAndWaitForPause(
    () => evaluateTestCode(debuggee),
    threadFront
  );

  info("Test that we pause on the debugger statement.");
  Assert.equal(packet.frame.where.line, 4);
  Assert.equal(packet.why.type, "debuggerStatement");

  info("Add set watchpoint.");
  const args = packet.frame.arguments;
  const obj = args[0];
  const objClient = threadFront.pauseGrip(obj);
  await objClient.addWatchpoint("a", "obj.a", "set");

  info("Test that watchpoint triggers pause on set.");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 6);
  Assert.equal(packet2.why.type, "setWatchpoint");
  let result = await evaluateJS("obj.a");
  Assert.equal(result, 1);

  info("Test that watchpoint triggers pause on set (2nd time).");
  const packet3 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet3.frame.where.line, 6);
  Assert.equal(packet3.why.type, "setWatchpoint");
  let result2 = await evaluateJS("obj.a");
  Assert.equal(result2, 2);

  info("Test that we pause on second debugger statement.");
  const packet4 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet4.frame.where.line, 8);
  Assert.equal(packet4.why.type, "debuggerStatement");

  await resume(threadFront);
}
