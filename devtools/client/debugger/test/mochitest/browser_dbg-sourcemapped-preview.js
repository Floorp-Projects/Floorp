/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests for preview through Babel's compile output.
requestLongerTimeout(3);

add_task(async function() {
  const dbg = await initDebugger("doc-sourcemapped.html");
  dbg.actions.toggleMapScopes();

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
  const filename = `${target}://./${fixture}/input.`;
  const fnName = (target + "-" + fixture).replace(/-([a-z])/g, (s, c) =>
    c.toUpperCase()
  );

  log(`Starting ${fixture} tests`);

  await invokeWithBreakpoint(
    dbg,
    fnName,
    filename,
    { line, column },
    async () => {
      await assertPreviews(dbg, previews);
    }
  );

  ok(true, `Ran tests for ${fixture} at line ${line} column ${column}`);
}

function testForOf(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "for-of",
    { line: 5, column: 4 },
    [
      {
        line: 5,
        column: 7,
        expression: "doThing",
        result: "doThing(arg)"
      },
      {
        line: 5,
        column: 13,
        expression: "x",
        result: "1"
      },
      {
        line: 8,
        column: 16,
        expression: "doThing",
        result: "doThing(arg)"
      }
    ]
  );
}

function testShadowing(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "shadowed-vars",
    { line: 18, column: 6 },
    [
      // These aren't what the user would expect, but we test them anyway since
      // they reflect what this actually returns. These shadowed bindings read
      // the binding closest to the current frame's scope even though their
      // actual value is different.
      {
        line: 2,
        column: 9,
        expression: "aVar",
        result: '"var3"'
      },
      {
        line: 3,
        column: 9,
        expression: "_aLet2;",
        result: '"let3"'
      },
      {
        line: 4,
        column: 11,
        expression: "_aConst2;",
        result: '"const3"'
      },
      {
        line: 10,
        column: 11,
        expression: "aVar",
        result: '"var3"'
      },
      {
        line: 11,
        column: 11,
        expression: "_aLet2;",
        result: '"let3"'
      },
      {
        line: 12,
        column: 13,
        expression: "_aConst2;",
        result: '"const3"'
      },

      // These actually result in the values the user would expect.
      {
        line: 14,
        column: 13,
        expression: "aVar",
        result: '"var3"'
      },
      {
        line: 15,
        column: 13,
        expression: "_aLet2;",
        result: '"let3"'
      },
      {
        line: 16,
        column: 13,
        expression: "_aConst2;",
        result: '"const3"'
      }
    ]
  );
}

function testImportedBindings(dbg) {
  return breakpointPreviews(
    dbg,
    "webpack3-babel6",
    "esmodules-cjs",
    { line: 20, column: 2 },
    [
      {
        line: 20,
        column: 16,
        expression: "_mod2.default;",
        result: '"a-default"'
      },
      {
        line: 21,
        column: 16,
        expression: "_mod4.original;",
        result: '"an-original"'
      },
      {
        line: 22,
        column: 16,
        expression: "_mod3.aNamed;",
        result: '"a-named"'
      },
      {
        line: 23,
        column: 16,
        expression: "_mod3.aNamed;",
        result: '"a-named"'
      },
      {
        line: 24,
        column: 16,
        expression: "aNamespace",
        fields: [["aNamed", "a-named"], ["default", "a-default"]]
      },
      {
        line: 29,
        column: 20,
        expression: "_mod7.default;",
        result: '"a-default2"'
      },
      {
        line: 30,
        column: 20,
        expression: "_mod9.original;",
        result: '"an-original2"'
      },
      {
        line: 31,
        column: 20,
        expression: "_mod8.aNamed2;",
        result: '"a-named2"'
      },
      {
        line: 32,
        column: 20,
        expression: "_mod8.aNamed2;",
        result: '"a-named2"'
      }
    ]
  );
}
