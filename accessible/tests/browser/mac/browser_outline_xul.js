/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  "mac/doc_tree.xhtml",
  async (browser, accDoc) => {
    const tree = getNativeInterface(accDoc, "tree");
    is(
      tree.getAttributeValue("AXRole"),
      "AXOutline",
      "Found tree with role outline"
    );
    // XUL trees store all rows as direct children of the outline,
    // so we should see nine here instead of just three:
    // (Groceries, Fruits, Veggies)
    const treeChildren = tree.getAttributeValue("AXChildren");
    is(treeChildren.length, 9, "Found nine direct children");

    const treeCols = tree.getAttributeValue("AXColumns");
    is(treeCols.length, 1, "Found one column in tree");

    // Here, we should get only outline rows, not the title
    const treeRows = tree.getAttributeValue("AXRows");
    is(treeRows.length, 8, "Found 8 total rows");

    is(
      treeRows[0].getAttributeValue("AXDescription"),
      "Fruits",
      "Located correct first row, row has correct desc"
    );
    is(
      treeRows[0].getAttributeValue("AXDisclosing"),
      1,
      "Fruits is disclosing"
    );
    is(
      treeRows[0].getAttributeValue("AXDisclosedByRow"),
      null,
      "Fruits is disclosed by outline"
    );
    is(
      treeRows[0].getAttributeValue("AXDisclosureLevel"),
      0,
      "Fruits is level zero"
    );
    let disclosedRows = treeRows[0].getAttributeValue("AXDisclosedRows");
    is(disclosedRows.length, 2, "Fruits discloses two rows");
    is(
      disclosedRows[0].getAttributeValue("AXDescription"),
      "Apple",
      "fruits discloses apple"
    );
    is(
      disclosedRows[1].getAttributeValue("AXDescription"),
      "Orange",
      "fruits discloses orange"
    );

    is(
      treeRows[1].getAttributeValue("AXDescription"),
      "Apple",
      "Located correct second row, row has correct desc"
    );
    is(
      treeRows[1].getAttributeValue("AXDisclosing"),
      0,
      "Apple is not disclosing"
    );
    is(
      treeRows[1]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Fruits",
      "Apple is disclosed by fruits"
    );
    is(
      treeRows[1].getAttributeValue("AXDisclosureLevel"),
      1,
      "Apple is level one"
    );
    is(
      treeRows[1].getAttributeValue("AXDisclosedRows").length,
      0,
      "Apple does not disclose rows"
    );

    is(
      treeRows[2].getAttributeValue("AXDescription"),
      "Orange",
      "Located correct third row, row has correct desc"
    );
    is(
      treeRows[2].getAttributeValue("AXDisclosing"),
      0,
      "Orange is not disclosing"
    );
    is(
      treeRows[2]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Fruits",
      "Orange is disclosed by fruits"
    );
    is(
      treeRows[2].getAttributeValue("AXDisclosureLevel"),
      1,
      "Orange is level one"
    );
    is(
      treeRows[2].getAttributeValue("AXDisclosedRows").length,
      0,
      "Orange does not disclose rows"
    );

    is(
      treeRows[3].getAttributeValue("AXDescription"),
      "Veggies",
      "Located correct fourth row, row has correct desc"
    );
    is(
      treeRows[3].getAttributeValue("AXDisclosing"),
      1,
      "Veggies is disclosing"
    );
    is(
      treeRows[3].getAttributeValue("AXDisclosedByRow"),
      null,
      "Veggies is disclosed by outline"
    );
    is(
      treeRows[3].getAttributeValue("AXDisclosureLevel"),
      0,
      "Veggies is level zero"
    );
    disclosedRows = treeRows[3].getAttributeValue("AXDisclosedRows");
    is(disclosedRows.length, 2, "Veggies discloses two rows");
    is(
      disclosedRows[0].getAttributeValue("AXDescription"),
      "Green Veggies",
      "Veggies discloses green veggies"
    );
    is(
      disclosedRows[1].getAttributeValue("AXDescription"),
      "Squash",
      "Veggies discloses squash"
    );

    is(
      treeRows[4].getAttributeValue("AXDescription"),
      "Green Veggies",
      "Located correct fifth row, row has correct desc"
    );
    is(
      treeRows[4].getAttributeValue("AXDisclosing"),
      1,
      "Green veggies is disclosing"
    );
    is(
      treeRows[4]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Veggies",
      "Green Veggies is disclosed by veggies"
    );
    is(
      treeRows[4].getAttributeValue("AXDisclosureLevel"),
      1,
      "Green veggies is level one"
    );
    disclosedRows = treeRows[4].getAttributeValue("AXDisclosedRows");
    is(disclosedRows.length, 2, "Green veggies has two rows");
    is(
      disclosedRows[0].getAttributeValue("AXDescription"),
      "Spinach",
      "Green veggies discloses spinach"
    );
    is(
      disclosedRows[1].getAttributeValue("AXDescription"),
      "Peas",
      "Green veggies discloses peas"
    );

    is(
      treeRows[5].getAttributeValue("AXDescription"),
      "Spinach",
      "Located correct sixth row, row has correct desc"
    );
    is(
      treeRows[5].getAttributeValue("AXDisclosing"),
      0,
      "Spinach is not disclosing"
    );
    is(
      treeRows[5]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Green Veggies",
      "Spinach is disclosed by green veggies"
    );
    is(
      treeRows[5].getAttributeValue("AXDisclosureLevel"),
      2,
      "Spinach is level two"
    );
    is(
      treeRows[5].getAttributeValue("AXDisclosedRows").length,
      0,
      "Spinach does not disclose rows"
    );

    is(
      treeRows[6].getAttributeValue("AXDescription"),
      "Peas",
      "Located correct seventh row, row has correct desc"
    );
    is(
      treeRows[6].getAttributeValue("AXDisclosing"),
      0,
      "Peas is not disclosing"
    );
    is(
      treeRows[6]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Green Veggies",
      "Peas is disclosed by green veggies"
    );
    is(
      treeRows[6].getAttributeValue("AXDisclosureLevel"),
      2,
      "Peas is level two"
    );
    is(
      treeRows[6].getAttributeValue("AXDisclosedRows").length,
      0,
      "Peas does not disclose rows"
    );

    is(
      treeRows[7].getAttributeValue("AXDescription"),
      "Squash",
      "Located correct eighth row, row has correct desc"
    );
    is(
      treeRows[7].getAttributeValue("AXDisclosing"),
      0,
      "Squash is not disclosing"
    );
    is(
      treeRows[7]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      "Veggies",
      "Squash is disclosed by veggies"
    );
    is(
      treeRows[7].getAttributeValue("AXDisclosureLevel"),
      1,
      "Squash is level one"
    );
    is(
      treeRows[7].getAttributeValue("AXDisclosedRows").length,
      0,
      "Squash does not disclose rows"
    );
  },
  { topLevel: false, chrome: true }
);
