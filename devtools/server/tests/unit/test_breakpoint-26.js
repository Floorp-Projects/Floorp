/* eslint-disable max-nested-callbacks */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 925269 - Verify that debugger statements are skipped
 * if there is a falsey conditional breakpoint at the same location.
 */
add_task(
  threadFrontTest(async props => {
    await testBreakpointsAndDebuggerStatements(props);
  })
);

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
      column: 6,
    },
    { condition: "false" }
  );

  await performActions(threadFront, [
    [
      "paused at first debugger statement",
      { line: 2, type: "debuggerStatement" },
      "resume",
    ],
    [
      "pause at the third debugger statement",
      { line: 4, type: "debuggerStatement" },
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
