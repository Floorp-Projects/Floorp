async function evalInConsoleAtPoint(
  dbg,
  fixture,
  { line, column },
  statements
) {
  const filename = `fixtures/${fixture}/input.js`;
  const fnName = fixture.replace(/-([a-z])/g, (s, c) => c.toUpperCase());

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
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("doc-sourcemapped.html");

  await evalInConsoleAtPoint(dbg, "babel-eval-maps", { line: 14, column: 4 }, [
    "one === 1",
    "two === 4",
    "three === 5"
  ]);

  await evalInConsoleAtPoint(
    dbg,
    "babel-modules-cjs",
    { line: 20, column: 2 },
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
    "babel-shadowed-vars",
    { line: 18, column: 6 },
    [`aVar === "var3"`, `aLet === "let3"`, `aConst === "const3"`]
  );
});
