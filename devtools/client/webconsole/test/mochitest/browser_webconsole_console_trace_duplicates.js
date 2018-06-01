/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/mochitest/" +
                  "test-console-trace-duplicates.html";

add_task(async function testTraceMessages() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // NB: Now that stack frames include a column number multiple invocations
  //     on the same line are considered unique. ie:
  //       |foo(); foo();|
  //     will generate two distinct trace entries.
  const message = await waitFor(() => findMessage(hud, "foo1"));
  const stackInfo = getStackInfo(message);

  checkStackInfo(stackInfo, {
    variable: "console.trace()",
    repeats: 3,
    filename: "test-console-trace-duplicates.html",
    line: 24,
    column: 3,
    stack: [{
      functionName: "foo3",
      filename: TEST_URI,
      line: 24,
      column: 3
    }, {
      functionName: "foo2",
      filename: TEST_URI,
      line: 20,
      column: 3
    }, {
      functionName: "foo1",
      filename: TEST_URI,
      line: 12,
      column: 3
    }, {
      functionName: "<anonymous>",
      filename: TEST_URI,
      line: 27,
      column: 1
    }]
  });
});

/**
 * Get stack info from a message node. This is a companion to checkStackInfo().
 *
 * @param {MessageNode}
 *        The message from which the stack info will be returned.
 * @returns {Object}
 *          An object in the following format:
 *            {
 *              variable: "console.trace()",
 *              repeats: 3
 *              filename: "some-filename.html"
 *              line: 23
 *              column: 3
 *              stack: [
 *                {
 *                  "functionName": "foo3",
 *                  "filename": "http://example.com/some-filename.html",
 *                  "line":"23",
 *                  "column":"3"
 *                },
 *                ...
 *              ]
 *            }
 */
function getStackInfo(message) {
  const lineNode = message.querySelector(".frame-link-line");
  const lc = getLineAndColumn(lineNode);
  const result = {
    variable: message.querySelector(".cm-variable").textContent,
    repeats: message.querySelector(".message-repeats").textContent,
    filename: message.querySelector(".frame-link-filename").textContent,
    line: lc.line,
    column: lc.column,
    stack: []
  };

  const stack = message.querySelector(".stack-trace");
  if (stack) {
    const filenameNodes = stack.querySelectorAll(".frame-link-filename");
    const lineNodes = stack.querySelectorAll(".frame-link-line");
    const funcNodes = stack.querySelectorAll(".frame-link-function-display-name");

    for (let i = 0; i < filenameNodes.length; i++) {
      const filename = filenameNodes[i].textContent;
      const functionName = funcNodes[i].textContent;
      const { line, column } = getLineAndColumn(lineNodes[i]);

      result.stack.push({
        functionName,
        filename,
        line: line,
        column: column
      });
    }
  }

  return result;
}

/**
 * Check stack info returned by getStackInfo().
 *
 * @param {Object} stackInfo
 *        A stackInfo object returned by getStackInfo().
 * @param {Object} expected
 *        An object in the same format as the expected stackInfo object.
 */
function checkStackInfo(stackInfo, expected) {
  is(stackInfo.variable, expected.variable, `"$(expected.variable}" command logged`);
  is(stackInfo.repeats, expected.repeats, "expected number of repeats are displayed");
  is(stackInfo.filename, expected.filename, "expected filename is displayed");
  is(stackInfo.line, expected.line, "expected line is displayed");
  is(stackInfo.column, expected.column, "expected column is displayed");

  ok(stackInfo.stack.length > 0, "a stack is displayed");
  is(stackInfo.stack.length, expected.stack.length, "the stack is the expected length");

  for (let i = 0; i < stackInfo.stack.length; i++) {
    const actual = stackInfo.stack[i];
    const stackExpected = expected.stack[i];

    is(actual.functionName, stackExpected.functionName,
      `expected function name is displayed for index ${i}`);
    is(actual.filename, stackExpected.filename,
      `expected filename is displayed for index ${i}`);
    is(actual.line, stackExpected.line,
      `expected line is displayed for index ${i}`);
    is(actual.column, stackExpected.column,
      `expected column is displayed for index ${i}`);
  }
}

/**
 * Splits the line and column info from the node containing the line and column
 * to be split e.g. ':10:15'.
 *
 * @param {HTMLNode} node
 *        The HTML node containing the line and column to be split.
 */
function getLineAndColumn(node) {
  let lineAndColumn = node.textContent;
  let line = 0;
  let column = 0;

  // LineAndColumn should look something like ":10:15"
  if (lineAndColumn.startsWith(":")) {
    // Trim the first colon leaving e.g. "10:15"
    lineAndColumn = lineAndColumn.substr(1);

    // Split "10:15" into line and column variables.
    [ line, column ] = lineAndColumn.split(":");
  }

  return {
    line: line * 1,
    column: column * 1
  };
}
