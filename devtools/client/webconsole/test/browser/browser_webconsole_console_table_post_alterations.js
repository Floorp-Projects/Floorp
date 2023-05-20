/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that calling console.table on a variable which is modified after the
// console.table call only shows data for when the variable was logged.

const TEST_URI = `data:text/html,<!DOCTYPE html>Test console.table with modified variable`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    const x = ["a", "b"];
    content.wrappedJSObject.console.table(x);
    x.push("c");
    content.wrappedJSObject.console.table(x);
    x.sort((a, b) => {
      if (a < b) {
        return 1;
      }
      if (a > b) {
        return -1;
      }
      return 0;
    });
    content.wrappedJSObject.console.table(x);
  });

  const [table1, table2, table3] = await waitFor(() => {
    const res = hud.ui.outputNode.querySelectorAll(".message .consoletable");
    if (res.length === 3) {
      return res;
    }
    return null;
  });

  info("Check the rows of the first table");
  checkTable(table1, [
    [0, "a"],
    [1, "b"],
  ]);

  info("Check the rows of the table after adding an element to the array");
  checkTable(table2, [
    [0, "a"],
    [1, "b"],
    [2, "c"],
  ]);

  info("Check the rows of the table after sorting the array");
  checkTable(table3, [
    [0, "c"],
    [1, "b"],
    [2, "a"],
  ]);
});

function checkTable(node, expectedRows) {
  const rows = Array.from(node.querySelectorAll("tbody tr"));
  is(rows.length, expectedRows.length, "table has the expected number of rows");

  expectedRows.forEach((expectedRow, rowIndex) => {
    const rowCells = Array.from(rows[rowIndex].querySelectorAll("td"));
    is(rowCells.map(x => x.textContent).join(" | "), expectedRow.join(" | "));
  });
}
