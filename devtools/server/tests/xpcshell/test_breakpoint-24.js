/* eslint-disable max-nested-callbacks */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1441183 - Verify that the debugger advances to a new location
 * when encountering debugger statements and brakpoints
 *
 * Bug 1613165 - Verify that debugger statement is not disabled by
 *  adding/removing a breakpoint
 */
add_task(
  threadFrontTest(async props => {
    await testDebuggerStatements(props);
    await testBreakpoints(props);
    await testBreakpointsAndDebuggerStatements(props);
    await testLoops(props);
    await testRemovingBreakpoint(props);
    await testAddingBreakpoint(props);
  })
);

// Ensure that we advance to the next line when we
// step to a debugger statement and resume.
async function testDebuggerStatements({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      debugger;
      debugger;
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/code.js`);

  await performActions(threadFront, [
    [
      "paused at first debugger statement",
      { line: 2, type: "debuggerStatement" },
      "stepOver",
    ],
    [
      "paused at the second debugger statement",
      { line: 3, type: "resumeLimit" },
      "resume",
    ],
    [
      "paused at the third debugger statement",
      { line: 4, type: "debuggerStatement" },
      "resume",
    ],
  ]);
}

// Ensure that we advance to the next line when we hit a breakpoint
// on a line with a debugger statement and resume.
async function testBreakpointsAndDebuggerStatements({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      debugger;
      debugger;
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/testBreakpointsAndDebuggerStatements.js`);

  threadFront.setBreakpoint(
    {
      sourceUrl: "http://example.com/testBreakpointsAndDebuggerStatements.js",
      line: 3,
    },
    {}
  );

  await performActions(threadFront, [
    [
      "paused at first debugger statement",
      { line: 2, type: "debuggerStatement" },
      "resume",
    ],
    [
      "paused at the breakpoint at the second debugger statement",
      { line: 3, type: "breakpoint" },
      "resume",
    ],
    [
      "pause at the third debugger statement",
      { line: 4, type: "debuggerStatement" },
      "resume",
    ],
  ]);
}

// Ensure that we advance to the next line when we step to
// a line with a breakpoint and resume.
async function testBreakpoints({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      debugger;
      a();
      debugger;
    }
    function a() {}
    foo();
    //# sourceURL=http://example.com/testBreakpoints.js`);

  threadFront.setBreakpoint(
    { sourceUrl: "http://example.com/testBreakpoints.js", line: 3, column: 6 },
    {}
  );

  await performActions(threadFront, [
    [
      "paused at first debugger statement",
      { line: 2, type: "debuggerStatement" },
      "stepOver",
    ],
    ["paused at a()", { line: 3, type: "resumeLimit" }, "resume"],
    [
      "pause at the second debugger satement",
      { line: 4, type: "debuggerStatement" },
      "resume",
    ],
  ]);
}

// Ensure that we advance to the next line when we step to
// a line with a breakpoint and resume.
async function testLoops({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      let i = 0;
      debugger;
      while (i++ < 2) {
        debugger;
      }
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/testLoops.js`);

  await performActions(threadFront, [
    [
      "paused at first debugger statement",
      { line: 3, type: "debuggerStatement" },
      "resume",
    ],
    [
      "pause at the second debugger satement",
      { line: 5, type: "debuggerStatement" },
      "resume",
    ],
    [
      "pause at the second debugger satement (2nd time)",
      { line: 5, type: "debuggerStatement" },
      "resume",
    ],
    [
      "pause at the third debugger satement",
      { line: 7, type: "debuggerStatement" },
      "resume",
    ],
  ]);
}

// Bug 1613165 - ensure that if you pause on a breakpoint on a line with
// debugger statement, remove the breakpoint, and try to pause on the
// debugger statement before pausing anywhere else, debugger pauses instead of
// skipping debugger statement.
async function testRemovingBreakpoint({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      debugger;
    }
    foo();
    foo();
    //# sourceURL=http://example.com/testRemovingBreakpoint.js`);

  const location = {
    sourceUrl: "http://example.com/testRemovingBreakpoint.js",
    line: 2,
    column: 6,
  };

  threadFront.setBreakpoint(location, {});

  info("paused at the breakpoint at the first debugger statement");
  const packet = await waitForEvent(threadFront, "paused");
  Assert.equal(packet.frame.where.line, 2);
  Assert.equal(packet.why.type, "breakpoint");
  threadFront.removeBreakpoint(location);

  info("paused at the first debugger statement");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 2);
  Assert.equal(packet2.why.type, "debuggerStatement");
  await threadFront.resume();
}

// Bug 1613165 - ensure if you pause on a debugger statement, add a
// breakpoint on the same line, and try to pause on the breakpoint
// before pausing anywhere else, debugger pauses on that line instead of
// skipping breakpoint.
async function testAddingBreakpoint({ commands, threadFront }) {
  commands.scriptCommand.execute(`function foo(stop) {
      debugger;
    }
    foo();
    foo();
    //# sourceURL=http://example.com/testAddingBreakpoint.js`);

  const location = {
    sourceUrl: "http://example.com/testAddingBreakpoint.js",
    line: 2,
    column: 6,
  };

  info("paused at the first debugger statement");
  const packet = await waitForEvent(threadFront, "paused");
  Assert.equal(packet.frame.where.line, 2);
  Assert.equal(packet.why.type, "debuggerStatement");
  threadFront.setBreakpoint(location, {});

  info("paused at the breakpoint at the first debugger statement");
  const packet2 = await resumeAndWaitForPause(threadFront);
  Assert.equal(packet2.frame.where.line, 2);
  Assert.equal(packet2.why.type, "breakpoint");
  await threadFront.resume();
}

async function performActions(threadFront, actions) {
  for (const action of actions) {
    await performAction(threadFront, action);
  }
}

async function performAction(threadFront, [description, result, action]) {
  info(description);
  const packet = await waitForEvent(threadFront, "paused");
  Assert.equal(packet.frame.where.line, result.line);
  Assert.equal(packet.why.type, result.type);
  await threadFront[action]();
}
