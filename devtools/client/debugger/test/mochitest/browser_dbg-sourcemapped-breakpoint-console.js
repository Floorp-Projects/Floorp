/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test can be really slow on debug platforms and should be split.
requestLongerTimeout(3);

async function evalInConsoleAtPoint(
  dbg,
  target,
  fixture,
  { line, column },
  statements
) {
  const filename = `${target}://./${fixture}/input.`;
  const fnName = (target + "-" + fixture).replace(/-([a-z])/g, (s, c) =>
    c.toUpperCase()
  );

  await invokeWithBreakpoint(
    dbg,
    fnName,
    filename,
    { line, column },
    async () => {
      await assertConsoleEval(dbg, statements);
    }
  );

  ok(true, `Ran tests for ${fixture} at line ${line} column ${column}`);
}

async function assertConsoleEval(dbg, statements) {
  const jsterm = (await dbg.toolbox.selectTool("webconsole")).hud.jsterm;

  for (const [index, statement] of statements.entries()) {
    await dbg.client.evaluate(`
      window.TEST_RESULT = false;
    `);
    await jsterm.execute(`
      TEST_RESULT = ${statement};
    `);

    const result = await dbg.client.evaluate(`window.TEST_RESULT`);
    is(result.result, true, `'${statement}' evaluates to true`);
  }
}

add_task(async function() {
  const dbg = await initDebugger("doc-sourcemapped.html");
  dbg.actions.toggleMapScopes();

  await evalInConsoleAtPoint(
    dbg,
    "webpack3-babel6",
    "eval-maps",
    { line: 14, column: 4 },
    ["one === 1", "two === 4", "three === 5"]
  );

  await evalInConsoleAtPoint(
    dbg,
    "webpack3-babel6",
    "esmodules-cjs",
    { line: 18, column: 2 },
    [
      `aDefault === "a-default"`,
      `anAliased === "an-original"`,
      `aNamed === "a-named"`,
      `aDefault2 === "a-default2"`,
      `anAliased2 === "an-original2"`,
      `aNamed2 === "a-named2"`,
      `aDefault3 === "a-default3"`,
      `anAliased3 === "an-original3"`,
      `aNamed3 === "a-named3"`
    ]
  );

  await evalInConsoleAtPoint(
    dbg,
    "webpack3-babel6",
    "shadowed-vars",
    { line: 18, column: 6 },
    [`aVar === "var3"`, `aLet === "let3"`, `aConst === "const3"`]
  );

  await evalInConsoleAtPoint(
    dbg,
    "webpack3-babel6",
    "babel-classes",
    { line: 8, column: 16 },
    [`this.hasOwnProperty("bound")`]
  );
});
