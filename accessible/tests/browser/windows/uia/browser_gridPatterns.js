/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable camelcase */
const RowOrColumnMajor_RowMajor = 0;
/* eslint-enable camelcase */

const SNIPPET = `
<table id="table">
  <tr><th id="a">a</th><th id="b">b</th><th id="c">c</th></tr>
  <tr><th id="dg" rowspan="2">dg</th><td id="ef" colspan="2" headers="b c">ef</td></tr>
  <tr><th id="h">h</th><td id="i" headers="dg h">i</td></tr>
  <tr><td id="jkl" colspan="3" headers="a b c">jkl</td></tr>
</table>
<button id="button">button</button>
`;

async function testGridGetItem(row, col, cellId) {
  is(
    await runPython(`pattern.GetItem(${row}, ${col}).CurrentAutomationId`),
    cellId,
    `GetItem with row ${row} and col ${col} returned ${cellId}`
  );
}

async function testGridItemProps(id, row, col, rowSpan, colSpan, gridId) {
  await assignPyVarToUiaWithId(id);
  await definePyVar("pattern", `getUiaPattern(${id}, "GridItem")`);
  ok(await runPython(`bool(pattern)`), `${id} has GridItem pattern`);
  is(await runPython(`pattern.CurrentRow`), row, `${id} has correct Row`);
  is(await runPython(`pattern.CurrentColumn`), col, `${id} has correct Column`);
  is(
    await runPython(`pattern.CurrentRowSpan`),
    rowSpan,
    `${id} has correct RowSpan`
  );
  is(
    await runPython(`pattern.CurrentColumnSpan`),
    colSpan,
    `${id} has correct ColumnSpan`
  );
  is(
    await runPython(`pattern.CurrentContainingGrid.CurrentAutomationId`),
    gridId,
    `${id} ContainingGridItem is ${gridId}`
  );
}

async function testTableItemProps(id, rowHeaders, colHeaders) {
  await assignPyVarToUiaWithId(id);
  await definePyVar("pattern", `getUiaPattern(${id}, "TableItem")`);
  ok(await runPython(`bool(pattern)`), `${id} has TableItem pattern`);
  await isUiaElementArray(
    `pattern.GetCurrentRowHeaderItems()`,
    rowHeaders,
    `${id} has correct RowHeaderItems`
  );
  await isUiaElementArray(
    `pattern.GetCurrentColumnHeaderItems()`,
    colHeaders,
    `${id} has correct ColumnHeaderItems`
  );
}

/**
 * Test the Grid pattern.
 */
addUiaTask(SNIPPET, async function testGrid() {
  await definePyVar("doc", `getDocUia()`);
  await assignPyVarToUiaWithId("table");
  await definePyVar("pattern", `getUiaPattern(table, "Grid")`);
  ok(await runPython(`bool(pattern)`), "table has Grid pattern");
  is(
    await runPython(`pattern.CurrentRowCount`),
    4,
    "table has correct RowCount"
  );
  is(
    await runPython(`pattern.CurrentColumnCount`),
    3,
    "table has correct ColumnCount"
  );
  await testGridGetItem(0, 0, "a");
  await testGridGetItem(0, 1, "b");
  await testGridGetItem(0, 2, "c");
  await testGridGetItem(1, 0, "dg");
  await testGridGetItem(1, 1, "ef");
  await testGridGetItem(1, 2, "ef");
  await testGridGetItem(2, 0, "dg");
  await testGridGetItem(2, 1, "h");
  await testGridGetItem(2, 2, "i");

  await testPatternAbsent("button", "Grid");
});

/**
 * Test the GridItem pattern.
 */
addUiaTask(SNIPPET, async function testGridItem() {
  await definePyVar("doc", `getDocUia()`);
  await testGridItemProps("a", 0, 0, 1, 1, "table");
  await testGridItemProps("b", 0, 1, 1, 1, "table");
  await testGridItemProps("c", 0, 2, 1, 1, "table");
  await testGridItemProps("dg", 1, 0, 2, 1, "table");
  await testGridItemProps("ef", 1, 1, 1, 2, "table");
  await testGridItemProps("jkl", 3, 0, 1, 3, "table");

  await testPatternAbsent("button", "GridItem");
});

/**
 * Test the Table pattern.
 */
addUiaTask(
  SNIPPET,
  async function testTable() {
    await definePyVar("doc", `getDocUia()`);
    await assignPyVarToUiaWithId("table");
    await definePyVar("pattern", `getUiaPattern(table, "Table")`);
    ok(await runPython(`bool(pattern)`), "table has Table pattern");
    await isUiaElementArray(
      `pattern.GetCurrentRowHeaders()`,
      ["dg", "h"],
      "table has correct RowHeaders"
    );
    await isUiaElementArray(
      `pattern.GetCurrentColumnHeaders()`,
      ["a", "b", "c"],
      "table has correct ColumnHeaders"
    );
    is(
      await runPython(`pattern.CurrentRowOrColumnMajor`),
      RowOrColumnMajor_RowMajor,
      "table has correct RowOrColumnMajor"
    );

    await testPatternAbsent("button", "Table");
  },
  // The IA2 -> UIA proxy doesn't support the Row/ColumnHeaders properties.
  { uiaEnabled: true, uiaDisabled: false }
);

/**
 * Test the TableItem pattern.
 */
addUiaTask(SNIPPET, async function testTableItem() {
  await definePyVar("doc", `getDocUia()`);
  await testTableItemProps("a", [], []);
  await testTableItemProps("b", [], []);
  await testTableItemProps("c", [], []);
  await testTableItemProps("dg", [], ["a"]);
  await testTableItemProps("ef", ["dg"], ["b", "c"]);
  await testTableItemProps("h", ["dg"], ["b"]);
  await testTableItemProps("i", ["dg", "h"], ["c"]);
  await testTableItemProps("jkl", [], ["a", "b", "c"]);

  await testPatternAbsent("button", "TableItem");
});
