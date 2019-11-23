/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that calling console.table on a variable which is modified after the
// console.table call only shows data for when the variable was logged.

const TEST_URI = `data:text/html,Test console.table with modified variable`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    const x = ["a", "b"];
    content.wrappedJSObject.console.table(x);
    x.push("c");
    content.wrappedJSObject.console.table(x);
    x.sort((a, b) => b - a);
    content.wrappedJSObject.console.table(x);
  });

  const [table1, table2, table3] = await waitFor(() => {
    const res = hud.ui.outputNode.querySelectorAll(
      ".message .new-consoletable"
    );
    if (res.length === 3) {
      return res;
    }
    return null;
  });

  info("Check the rows of the first table");
  checkTable(table1, [[0, "a"], [1, "b"]]);

  info("Check the rows of the table after adding an element to the array");
  checkTable(table2, [[0, "a"], [1, "b"], [2, "c"]]);

  info("Check the rows of the table after sorting the array");
  checkTable(table3, [[0, "c"], [1, "b"], [2, "a"]]);
});

function checkTable(node, expectedRows) {
  const columns = Array.from(node.querySelectorAll("[role=columnheader]"));
  const columnsNumber = columns.length;
  const cells = Array.from(node.querySelectorAll("[role=gridcell]"));

  // We don't really have rows since we are using a CSS grid in order to have a sticky
  // header on the table. So we check the "rows" by dividing the number of cells by the
  // number of columns.
  is(
    cells.length / columnsNumber,
    expectedRows.length,
    "table has the expected number of rows"
  );

  expectedRows.forEach((expectedRow, rowIndex) => {
    const startIndex = rowIndex * columnsNumber;
    // Slicing the cells array so we can get the current "row".
    const rowCells = cells.slice(startIndex, startIndex + columnsNumber);
    is(rowCells.map(x => x.textContent).join(" | "), expectedRow.join(" | "));
  });
}
