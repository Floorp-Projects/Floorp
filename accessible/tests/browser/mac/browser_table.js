/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

/**
 * Helper function to test table consistency.
 */
function testTableConsistency(table, expectedRowCount, expectedColumnCount) {
  is(table.getAttributeValue("AXRole"), "AXTable", "Correct role for table");

  let tableChildren = table.getAttributeValue("AXChildren");
  // XXX: Should be expectedRowCount+ExpectedColumnCount+1 children, rows (incl headers) + cols + headers
  // if we're trying to match Safari.
  is(
    tableChildren.length,
    expectedRowCount + expectedColumnCount,
    "Table has children = rows (4) + cols (3)"
  );
  for (let i = 0; i < tableChildren.length; i++) {
    let currChild = tableChildren[i];
    if (i < expectedRowCount) {
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
    expectedColumnCount,
    "Table has correct column count."
  );
  is(
    table.getAttributeValue("AXRowCount"),
    expectedRowCount,
    "Table has correct row count."
  );

  let cols = table.getAttributeValue("AXColumns");
  is(cols.length, expectedColumnCount, "Table has col list of correct length");
  for (let i = 0; i < cols.length; i++) {
    let currCol = cols[i];
    let currChildren = currCol.getAttributeValue("AXChildren");
    is(
      currChildren.length,
      expectedRowCount,
      "Column has correct number of cells"
    );
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
  is(rows.length, expectedRowCount, "Table has row list of correct length");
  for (let i = 0; i < rows.length; i++) {
    let currRow = rows[i];
    let currChildren = currRow.getAttributeValue("AXChildren");
    is(
      currChildren.length,
      expectedColumnCount,
      "Row has correct number of cells"
    );
    for (let j = 0; j < currChildren.length; j++) {
      let currChild = currChildren[j];
      is(currChild.getAttributeValue("AXRole"), "AXCell", "Row child is cell");
    }
  }
}

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
    testTableConsistency(table, 4, 3);

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

    let cols = table.getAttributeValue("AXColumns");
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

    reorder = waitForEvent(
      EVENT_REORDER,
      e => e.accessible.role == ROLE_DOCUMENT
    );
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("customers").remove();
    });
    await reorder;

    try {
      cols[0].getAttributeValue("AXChildren");
      ok(false, "Getting children from column of expired table should fail");
    } catch (e) {
      ok(true, "Getting children from column of expired table should fail");
    }
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

addAccessibleTask(
  `<div id="table" role="table">
    <span style="display: block;">
      <div role="row">
        <div role="cell">Cell 1</div>
        <div role="cell">Cell 2</div>
      </div>
    </span>
    <span style="display: block;">
      <div role="row">
        <span style="display: block;">
          <div role="cell">Cell 3</div>
          <div role="cell">Cell 4</div>
        </span>
      </div>
    </span>
  </div>`,
  async (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "table");
    testTableConsistency(table, 2, 2);
  }
);

/*
 * After executing function 'change' which operates on 'elem', verify the specified
 * 'event' is fired on the test's table (assumed id="table"). After the event, check
 * if the given native accessible 'table' is a layout or data table by role
 * using 'isLayout'.
 */
async function testIsLayout(table, elem, event, change, isLayout) {
  info(
    "Changing " +
      elem +
      ", expecting table change to " +
      (isLayout ? "AXGroup" : "AXTable")
  );
  const toWait = waitForEvent(
    event,
    event == EVENT_TABLE_STYLING_CHANGED ? "table" : elem
  );
  await change();
  await toWait;
  is(
    table.getAttributeValue("AXRole"),
    isLayout ? "AXGroup" : "AXTable",
    "Table role correct after change"
  );
}

/*
 * The following attributes should fire an attribute changed
 * event, which in turn invalidates the layout-table cache
 * associated with the given table. After adding and removing
 * each attr, verify the table is a data or layout table,
 * appropriately. Attrs: summary, abbr, scope, headers
 */
addAccessibleTask(
  `<table id="table" summary="example summary">
    <tr role="presentation">
      <td id="cellOne">cell1</td>
      <td>cell2</td>
    </tr>
    <tr>
      <td id="cellThree">cell3</td>
      <td>cell4</td>
    </tr>
  </table>`,
  async (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "table");
    // summary attr should take precedence over role="presentation" to make this
    // a data table
    is(table.getAttributeValue("AXRole"), "AXTable", "Table is data table");

    info("Removing summary attr");
    // after summary is removed, we should have a layout table
    await testIsLayout(
      table,
      "table",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document.getElementById("table").removeAttribute("summary");
        });
      },
      true
    );

    info("Setting abbr attr");
    // after abbr is set we should have a data table again
    await testIsLayout(
      table,
      "cellOne",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellOne")
            .setAttribute("abbr", "hello world");
        });
      },
      false
    );

    info("Removing abbr attr");
    // after abbr is removed we should have a layout table again
    await testIsLayout(
      table,
      "cellOne",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document.getElementById("cellOne").removeAttribute("abbr");
        });
      },
      true
    );

    info("Setting scope attr");
    // after scope is set we should have a data table again
    await testIsLayout(
      table,
      "cellOne",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellOne")
            .setAttribute("scope", "col");
        });
      },
      false
    );

    info("Removing scope attr");
    // remove scope should give layout
    await testIsLayout(
      table,
      "cellOne",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document.getElementById("cellOne").removeAttribute("scope");
        });
      },
      true
    );

    info("Setting headers attr");
    // add headers attr should give data
    await testIsLayout(
      table,
      "cellThree",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellThree")
            .setAttribute("headers", "cellOne");
        });
      },
      false
    );

    info("Removing headers attr");
    // remove headers attr should give layout
    await testIsLayout(
      table,
      "cellThree",
      EVENT_OBJECT_ATTRIBUTE_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellThree")
            .removeAttribute("headers");
        });
      },
      true
    );
  }
);

/*
 * The following style changes should fire a table style changed
 * event, which in turn invalidates the layout-table cache
 * associated with the given table.
 */
addAccessibleTask(
  `<table id="table">
    <tr id="rowOne">
      <td id="cellOne">cell1</td>
      <td>cell2</td>
    </tr>
    <tr>
      <td>cell3</td>
      <td>cell4</td>
    </tr>
  </table>`,
  async (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "table");
    // we should start as a layout table
    is(table.getAttributeValue("AXRole"), "AXGroup", "Table is layout table");

    info("Adding cell border");
    // after cell border added, we should have a data table
    await testIsLayout(
      table,
      "cellOne",
      EVENT_TABLE_STYLING_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellOne")
            .style.setProperty("border", "5px solid green");
        });
      },
      false
    );

    info("Removing cell border");
    // after cell border removed, we should have a layout table
    await testIsLayout(
      table,
      "cellOne",
      EVENT_TABLE_STYLING_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("cellOne")
            .style.removeProperty("border");
        });
      },
      true
    );

    info("Adding row background");
    // after row background added, we should have a data table
    await testIsLayout(
      table,
      "rowOne",
      EVENT_TABLE_STYLING_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("rowOne")
            .style.setProperty("background-color", "green");
        });
      },
      false
    );

    info("Removing row background");
    // after row background removed, we should have a layout table
    await testIsLayout(
      table,
      "rowOne",
      EVENT_TABLE_STYLING_CHANGED,
      async () => {
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .getElementById("rowOne")
            .style.removeProperty("background-color");
        });
      },
      true
    );
  }
);

/*
 * thead/tbody elements with click handlers should:
 * (a) render as AXGroup elements
 * (b) expose their rows as part of their parent table's AXRows array
 */
addAccessibleTask(
  `<table id="table">
    <thead id="thead">
      <tr><td>head row</td></tr>
    </thead>
    <tbody id="tbody">
      <tr><td>body row</td></tr>
      <tr><td>another body row</td></tr>
    </tbody>
  </table>`,
  async (browser, accDoc) => {
    let table = getNativeInterface(accDoc, "table");

    // No click handlers present on thead/tbody
    let tableChildren = table.getAttributeValue("AXChildren");
    let tableRows = table.getAttributeValue("AXRows");

    is(tableChildren.length, 4, "Table has four children (3 row + 1 col)");
    is(tableRows.length, 3, "Table has three rows");

    for (let i = 0; i < tableChildren.length; i++) {
      const child = tableChildren[i];
      if (i < 3) {
        is(
          child.getAttributeValue("AXRole"),
          "AXRow",
          "Table's first 3 children are rows"
        );
      } else {
        is(
          child.getAttributeValue("AXRole"),
          "AXColumn",
          "Table's last child is a column"
        );
      }
    }
    const reorder = waitForEvent(EVENT_REORDER);
    await invokeContentTask(browser, [], () => {
      const head = content.document.getElementById("thead");
      const body = content.document.getElementById("tbody");

      head.addEventListener("click", function() {});
      body.addEventListener("click", function() {});
    });
    await reorder;

    // Click handlers present
    tableChildren = table.getAttributeValue("AXChildren");

    is(tableChildren.length, 3, "Table has three children (2 groups + 1 col)");
    is(
      tableChildren[0].getAttributeValue("AXRole"),
      "AXGroup",
      "Child one is a group"
    );
    is(
      tableChildren[0].getAttributeValue("AXChildren").length,
      1,
      "Child one has one child"
    );

    is(
      tableChildren[1].getAttributeValue("AXRole"),
      "AXGroup",
      "Child two is a group"
    );
    is(
      tableChildren[1].getAttributeValue("AXChildren").length,
      2,
      "Child two has two children"
    );

    is(
      tableChildren[2].getAttributeValue("AXRole"),
      "AXColumn",
      "Child three is a col"
    );

    tableRows = table.getAttributeValue("AXRows");
    is(tableRows.length, 3, "Table has three rows");
  }
);
