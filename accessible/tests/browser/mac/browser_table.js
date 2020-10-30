/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

/**
 * Test table, columns, rows
 */
addAccessibleTask(
  `<table id="customers">
    <tbody>
      <tr id="firstrow"><th>Company</th><th>Contact</th><th>Country</th></tr>
      <tr><td>Alfreds Futterkiste</td><td>Maria Anders</td><td>Germany</td></tr>
      <tr><td>Centro comercial Moctezuma</td><td>Francisco Chang</td><td>Mexico</td></tr>
      <tr><td>Ernst Handel</td><td>Roland Mendel</td><td>Austria</td></tr>
    </tbody>
  </table>`,
  async (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "customers");
    is(table.getAttributeValue("AXRole"), "AXTable", "Correct role for table");

    let tableChildren = table.getAttributeValue("AXChildren");
    // XXX: Should be 8 children, rows (incl headers) + cols + headers
    // if we're trying to match Safari.
    is(tableChildren.length, 7, "Table has children = rows (4) + cols (3)");
    for (let i = 0; i < tableChildren.length; i++) {
      let currChild = tableChildren[i];
      if (i < 4) {
        is(
          currChild.getAttributeValue("AXRole"),
          "AXRow",
          "Correct role for row"
        );
      } else {
        is(
          currChild.getAttributeValue("AXRole"),
          "AXColumn",
          "Correct role for col"
        );
        is(
          currChild.getAttributeValue("AXRoleDescription"),
          "column",
          "Correct role desc for col"
        );
      }
    }

    is(
      table.getAttributeValue("AXColumnCount"),
      3,
      "Table has correct column count."
    );
    is(
      table.getAttributeValue("AXRowCount"),
      4,
      "Table has correct row count."
    );

    let cols = table.getAttributeValue("AXColumns");
    is(cols.length, 3, "Table has col list of correct length");
    for (let i = 0; i < cols.length; i++) {
      let currCol = cols[i];
      let currChildren = currCol.getAttributeValue("AXChildren");
      is(currChildren.length, 4, "Column has correct number of cells");
      for (let j = 0; j < currChildren.length; j++) {
        let currChild = currChildren[j];
        is(
          currChild.getAttributeValue("AXRole"),
          "AXCell",
          "Column child is cell"
        );
      }
    }

    let rows = table.getAttributeValue("AXRows");
    is(rows.length, 4, "Table has row list of correct length");
    for (let i = 0; i < rows.length; i++) {
      let currRow = rows[i];
      let currChildren = currRow.getAttributeValue("AXChildren");
      is(currChildren.length, 3, "Row has correct number of cells");
      for (let j = 0; j < currChildren.length; j++) {
        let currChild = currChildren[j];
        is(
          currChild.getAttributeValue("AXRole"),
          "AXCell",
          "Row child is cell"
        );
      }
    }

    const rowText = [
      "Madrigal Electromotive GmbH",
      "Lydia Rodarte-Quayle",
      "Germany",
    ];
    let reorder = waitForEvent(EVENT_REORDER, "customers");
    await SpecialPowers.spawn(browser, [rowText], _rowText => {
      let tr = content.document.createElement("tr");
      for (let t of _rowText) {
        let td = content.document.createElement("td");
        td.textContent = t;
        tr.appendChild(td);
      }
      content.document.getElementById("customers").appendChild(tr);
    });
    await reorder;

    cols = table.getAttributeValue("AXColumns");
    is(cols.length, 3, "Table has col list of correct length");
    for (let i = 0; i < cols.length; i++) {
      let currCol = cols[i];
      let currChildren = currCol.getAttributeValue("AXChildren");
      is(currChildren.length, 5, "Column has correct number of cells");
      let lastCell = currChildren[currChildren.length - 1];
      let cellChildren = lastCell.getAttributeValue("AXChildren");
      is(cellChildren.length, 1, "Cell has a single text child");
      is(
        cellChildren[0].getAttributeValue("AXRole"),
        "AXStaticText",
        "Correct role for cell child"
      );
      is(
        cellChildren[0].getAttributeValue("AXValue"),
        rowText[i],
        "Correct text for cell"
      );
    }

    reorder = waitForEvent(EVENT_REORDER, "firstrow");
    await SpecialPowers.spawn(browser, [], () => {
      let td = content.document.createElement("td");
      td.textContent = "Ticker";
      content.document.getElementById("firstrow").appendChild(td);
    });
    await reorder;

    cols = table.getAttributeValue("AXColumns");
    is(cols.length, 4, "Table has col list of correct length");
    is(
      cols[cols.length - 1].getAttributeValue("AXChildren").length,
      1,
      "Last column has single child"
    );
  }
);

addAccessibleTask(
  `<table id="table">
    <tr>
      <th colspan="2" id="header1">Header 1</th>
      <th id="header2">Header 2</th>
    </tr>
    <tr>
      <td id="cell1">one</td>
      <td id="cell2" rowspan="2">two</td>
      <td id="cell3">three</td>
    </tr>
    <tr>
    <td id="cell4">four</td>
    <td id="cell5">five</td>
  </tr>
  </table>`,
  (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "table");

    let getCellAt = (col, row) =>
      table.getParameterizedAttributeValue("AXCellForColumnAndRow", [col, row]);

    function testCell(cell, expectedId, expectedColRange, expectedRowRange) {
      is(
        cell.getAttributeValue("AXDOMIdentifier"),
        expectedId,
        "Correct DOM Identifier"
      );
      Assert.deepEqual(
        cell.getAttributeValue("AXColumnIndexRange"),
        expectedColRange,
        "Correct column range"
      );
      Assert.deepEqual(
        cell.getAttributeValue("AXRowIndexRange"),
        expectedRowRange,
        "Correct row range"
      );
    }

    testCell(getCellAt(0, 0), "header1", [0, 2], [0, 1]);
    testCell(getCellAt(1, 0), "header1", [0, 2], [0, 1]);
    testCell(getCellAt(2, 0), "header2", [2, 1], [0, 1]);

    testCell(getCellAt(0, 1), "cell1", [0, 1], [1, 1]);
    testCell(getCellAt(1, 1), "cell2", [1, 1], [1, 2]);
    testCell(getCellAt(2, 1), "cell3", [2, 1], [1, 1]);

    testCell(getCellAt(0, 2), "cell4", [0, 1], [2, 1]);
    testCell(getCellAt(1, 2), "cell2", [1, 1], [1, 2]);
    testCell(getCellAt(2, 2), "cell5", [2, 1], [2, 1]);

    let colHeaders = table.getAttributeValue("AXColumnHeaderUIElements");
    Assert.deepEqual(
      colHeaders.map(c => c.getAttributeValue("AXDOMIdentifier")),
      ["header1", "header1", "header2"],
      "Correct column headers"
    );
  }
);

addAccessibleTask(
  `<table id="table">
    <tr>
      <td>Foo</td>
    </tr>
  </table>`,
  (browser, accDoc) => {
    // Make sure we guess this table to be a layout table.
    testAttrs(
      findAccessibleChildByID(accDoc, "table"),
      { "layout-guess": "true" },
      true
    );

    let table = getNativeInterface(accDoc, "table");
    is(
      table.getAttributeValue("AXRole"),
      "AXGroup",
      "Correct role (AXGroup) for layout table"
    );

    let children = table.getAttributeValue("AXChildren");
    is(
      children.length,
      1,
      "Layout table has single child (no additional columns)"
    );
  }
);
