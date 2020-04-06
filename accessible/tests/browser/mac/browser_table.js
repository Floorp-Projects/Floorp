/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */

loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Test table, columns, rows
 */
addAccessibleTask(
  `<table id="customers">
    <tbody>
      <tr><th>Company</th><th>Contact</th><th>Country</th></tr>
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
  }
);
