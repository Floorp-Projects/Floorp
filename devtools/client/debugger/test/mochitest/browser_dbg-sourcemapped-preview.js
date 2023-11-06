/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests for preview through Babel's compile output.
requestLongerTimeout(3);

add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebugger("doc-sourcemapped.html");

  await testForOf(dbg);
  await testShadowing(dbg);
  await testImportedBindings(dbg);
});

async function breakpointPreviews(
  dbg,
  target,
  fixture,
  { line, column },
  previews
) {
  const url = `${target}://./${fixture}/input.js`;
  const fnName = `${target}-${fixture}`.replace(/-([a-z])/g, (s, c) =>
    c.toUpperCase()
  );

  info(`Starting ${fixture} tests`);

  await invokeWithBreakpoint(dbg, fnName, url, { line, column }, async () => {
    await assertPreviews(dbg, previews);
  });

  ok(true, `Ran tests for ${fixture} at line ${line} column ${column}`);
}

function testForOf(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "for-of",
    { line: 5, column: 5 },
    [
      {
        line: 5,
        column: 4,
        expression: "doThing",
        result: "doThing(arg)",
      },
      {
        line: 5,
        column: 13,
        expression: "x",
        result: "1",
      },
      {
        line: 8,
        column: 12,
        expression: "doThing",
        result: "doThing(arg)",
      },
    ]
  );
}

function testShadowing(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "shadowed-vars",
    { line: 18, column: 7 },
    [
      // These aren't what the user would expect, but we test them anyway since
      // they reflect what this actually returns. These shadowed bindings read
      // the binding closest to the current frame's scope even though their
      // actual value is different.
      {
        line: 2,
        column: 10,
        expression: "aVar",
        result: '"var3"',
      },
      {
        line: 3,
        column: 10,
        expression: "aLet",
        result: '"let3"',
      },
      {
        line: 4,
        column: 12,
        expression: "aConst",
        result: '"const3"',
      },
      {
        line: 10,
        column: 12,
        expression: "aVar",
        result: '"var3"',
      },
      {
        line: 11,
        column: 12,
        expression: "aLet",
        result: '"let3"',
      },
      {
        line: 12,
        column: 14,
        expression: "aConst",
        result: '"const3"',
      },

      // These actually result in the values the user would expect.
      {
        line: 14,
        column: 14,
        expression: "aVar",
        result: '"var3"',
      },
      {
        line: 15,
        column: 14,
        expression: "aLet",
        result: '"let3"',
      },
      {
        line: 16,
        column: 14,
        expression: "aConst",
        result: '"const3"',
      },
    ]
  );
}

function testImportedBindings(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "esmodules-cjs",
    { line: 20, column: 3 },
    [
      {
        line: 20,
        column: 17,
        expression: "aDefault",
        result: '"a-default"',
      },
      {
        line: 21,
        column: 17,
        expression: "anAliased",
        result: '"an-original"',
      },
      {
        line: 22,
        column: 17,
        expression: "aNamed",
        result: '"a-named"',
      },
      {
        line: 23,
        column: 17,
        expression: "anotherNamed",
        result: '"a-named"',
      },
      {
        line: 24,
        column: 17,
        expression: "aNamespace",
        fields: [
          ["aNamed", '"a-named"'],
          ["default", '"a-default"'],
        ],
      },
      {
        line: 29,
        column: 21,
        expression: "aDefault2",
        result: '"a-default2"',
      },
      {
        line: 30,
        column: 21,
        expression: "anAliased2",
        result: '"an-original2"',
      },
      {
        line: 31,
        column: 21,
        expression: "aNamed2",
        result: '"a-named2"',
      },
      {
        line: 32,
        column: 21,
        expression: "anotherNamed2",
        result: '"a-named2"',
      },
    ]
  );
}
