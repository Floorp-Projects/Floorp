/* eslint-disable max-nested-callbacks */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1441183 - Verify that the debugger advances to a new location
 * when encountering debugger statements and brakpoints
 */
add_task(
  threadFrontTest(async props => {
    await testDebuggerStatements(props);
    await testBreakpoints(props);
    await testBreakpointsAndDebuggerStatements(props);
    await testLoops(props);
  })
);

// Ensure that we advance to the next line when we
// step to a debugger statement and resume.
async function testDebuggerStatements({ threadFront, targetFront }) {
  const consoleFront = await targetFront.getFront("console");
  consoleFront.evaluateJSAsync(
    `function foo(stop) {
      debugger;
      debugger;
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/code.js`
  );

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
async function testBreakpointsAndDebuggerStatements({
  threadFront,
  targetFront,
}) {
  const consoleFront = await targetFront.getFront("console");
  consoleFront.evaluateJSAsync(
    `function foo(stop) {
      debugger;
      debugger;
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/testBreakpointsAndDebuggerStatements.js`
  );

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
async function testBreakpoints({ threadFront, targetFront }) {
  const consoleFront = await targetFront.getFront("console");
  consoleFront.evaluateJSAsync(
    `function foo(stop) {
      debugger;
      a();
      debugger;
    }
    function a() {}
    foo();
    //# sourceURL=http://example.com/testBreakpoints.js`
  );

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
async function testLoops({ threadFront, targetFront }) {
  const consoleFront = await targetFront.getFront("console");
  consoleFront.evaluateJSAsync(
    `function foo(stop) {
      let i = 0;
      debugger;
      while (i++ < 2) {
        debugger;
      }
      debugger;
    }
    foo();
    //# sourceURL=http://example.com/testLoops.js`
  );

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
