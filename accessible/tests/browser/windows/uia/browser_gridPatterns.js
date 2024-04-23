/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
