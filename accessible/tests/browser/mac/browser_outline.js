/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test outline, outline rows with computed properties
 */
addAccessibleTask(
  `
  <h3 id="tree1">
    Foods
  </h3>
  <ul role="tree" aria-labelledby="tree1" id="outline">
    <li role="treeitem" aria-expanded="false">
      <span>
        Fruits
      </span>
      <ul>
        <li role="none">Oranges</li>
        <li role="treeitem" aria-expanded="true">
          <span>
            Apples
          </span>
          <ul role="group">
            <li role="none">Honeycrisp</li>
            <li role="none">Granny Smith</li>
          </ul>
        </li>
      </ul>
    </li>
    <li id="vegetables" role="treeitem" aria-expanded="false">
      <span>
        Vegetables
      </span>
      <ul role="group">
        <li role="treeitem" aria-expanded="true">
          <span>
            Podded Vegetables
          </span>
          <ul role="group">
            <li role="none">Lentil</li>
            <li role="none">Pea</li>
          </ul>
        </li>
      </ul>
    </li>
  </ul>
  `,
  async (browser, accDoc) => {
    const outline = getNativeInterface(accDoc, "outline");
    is(
      outline.getAttributeValue("AXRole"),
      "AXOutline",
      "Correct role for outline"
    );

    const outChildren = outline.getAttributeValue("AXChildren");
    is(outChildren.length, 2, "Outline has two direct children");
    is(outChildren[0].getAttributeValue("AXSubrole"), "AXOutlineRow");
    is(outChildren[1].getAttributeValue("AXSubrole"), "AXOutlineRow");

    const outRows = outline.getAttributeValue("AXRows");
    is(outRows.length, 4, "Outline has four rows");
    is(
      outRows[0].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of outline"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no row children, only group"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosureLevel"),
      0,
      "Row is level zero"
    );

    is(outRows[1].getAttributeValue("AXDisclosing"), 1, "Row is disclosing");
    is(
      outRows[1].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of group"
    );
    is(
      outRows[1].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no row children"
    );
    is(
      outRows[1].getAttributeValue("AXDisclosureLevel"),
      0,
      "Row is level zero"
    );

    is(
      outRows[2].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[2].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of outline"
    );
    is(
      outRows[2].getAttributeValue("AXDisclosedRows").length,
      1,
      "Row has one row child"
    );
    is(
      outRows[2].getAttributeValue("AXDisclosureLevel"),
      0,
      "Row is level zero"
    );

    is(outRows[3].getAttributeValue("AXDisclosing"), 1, "Row is disclosing");
    is(
      outRows[3]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      outRows[2].getAttributeValue("AXDescription"),
      "Row is direct child of row[2]"
    );
    is(
      outRows[3].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no row children"
    );
    is(
      outRows[3].getAttributeValue("AXDisclosureLevel"),
      1,
      "Row is level one"
    );

    let evt = waitForMacEvent("AXRowExpanded", "vegetables");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("vegetables")
        .setAttribute("aria-expanded", "true");
    });
    await evt;
    is(
      outRows[2].getAttributeValue("AXDisclosing"),
      1,
      "Row is disclosing after being expanded"
    );

    evt = waitForMacEvent("AXRowCollapsed", "vegetables");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("vegetables")
        .setAttribute("aria-expanded", "false");
    });
    await evt;
    is(
      outRows[2].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing after being collapsed again"
    );
  }
);

/**
 * Test outline, outline rows with declared properties
 */
addAccessibleTask(
  `
  <h3 id="tree1">
  Foods
  </h3>
  <ul role="tree" aria-labelledby="tree1" id="outline">
    <li role="treeitem"
        aria-level="1"
        aria-setsize="2"
        aria-posinset="1"
        aria-expanded="false">
      <span>
        Fruits
      </span>
      <ul>
        <li role="treeitem"
             aria-level="3"
             aria-setsize="2"
             aria-posinset="1">
            Oranges
        </li>
        <li role="treeitem"
            aria-level="2"
            aria-setsize="2"
            aria-posinset="2"
            aria-expanded="true">
          <span>
            Apples
          </span>
          <ul role="group">
            <li role="treeitem"
                 aria-level="3"
                 aria-setsize="2"
                 aria-posinset="1">
                Honeycrisp
            </li>
            <li role="treeitem"
                 aria-level="3"
                 aria-setsize="2"
                 aria-posinset="2">
               Granny Smith
            </li>
          </ul>
        </li>
      </ul>
    </li>
    <li role="treeitem"
        aria-level="1"
        aria-setsize="2"
        aria-posinset="2"
        aria-expanded="false">
      <span>
        Vegetables
      </span>
      <ul role="group">
        <li role="treeitem"
            aria-level="2"
            aria-setsize="1"
            aria-posinset="1"
            aria-expanded="true">
          <span>
            Podded Vegetables
          </span>
          <ul role="group">
            <li role="treeitem"
                 aria-level="3"
                 aria-setsize="2"
                 aria-posinset="1">
               Lentil
            </li>
            <li role="treeitem"
                 aria-level="3"
                 aria-setsize="2"
                 aria-posinset="2">
                 Pea
            </li>
          </ul>
        </li>
      </ul>
    </li>
  </ul>
  `,
  async (browser, accDoc) => {
    const outline = getNativeInterface(accDoc, "outline");
    is(
      outline.getAttributeValue("AXRole"),
      "AXOutline",
      "Correct role for outline"
    );

    const outChildren = outline.getAttributeValue("AXChildren");
    is(outChildren.length, 2, "Outline has two direct children");
    is(outChildren[0].getAttributeValue("AXSubrole"), "AXOutlineRow");
    is(outChildren[1].getAttributeValue("AXSubrole"), "AXOutlineRow");

    const outRows = outline.getAttributeValue("AXRows");
    is(outRows.length, 9, "Outline has nine rows");
    is(
      outRows[0].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of outline"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no direct row children, has list"
    );
    is(
      outRows[0].getAttributeValue("AXDisclosureLevel"),
      0,
      "Row is level zero"
    );

    is(outRows[2].getAttributeValue("AXDisclosing"), 1, "Row is disclosing");
    is(
      outRows[2].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of group"
    );
    is(
      outRows[2].getAttributeValue("AXDisclosedRows").length,
      2,
      "Row has two row children"
    );
    is(
      outRows[2].getAttributeValue("AXDisclosureLevel"),
      1,
      "Row is level one"
    );

    is(
      outRows[3].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[3]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      outRows[2].getAttributeValue("AXDescription"),
      "Row is direct child of row 2"
    );

    is(
      outRows[3].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no row children"
    );
    is(
      outRows[3].getAttributeValue("AXDisclosureLevel"),
      2,
      "Row is level two"
    );

    is(
      outRows[5].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[5].getAttributeValue("AXDisclosedByRow"),
      null,
      "Row is direct child of outline"
    );
    is(
      outRows[5].getAttributeValue("AXDisclosedRows").length,
      1,
      "Row has no one row child"
    );
    is(
      outRows[5].getAttributeValue("AXDisclosureLevel"),
      0,
      "Row is level zero"
    );

    is(outRows[6].getAttributeValue("AXDisclosing"), 1, "Row is disclosing");
    is(
      outRows[6]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      outRows[5].getAttributeValue("AXDescription"),
      "Row is direct child of row 5"
    );
    is(
      outRows[6].getAttributeValue("AXDisclosedRows").length,
      2,
      "Row has two row children"
    );
    is(
      outRows[6].getAttributeValue("AXDisclosureLevel"),
      1,
      "Row is level one"
    );

    is(
      outRows[7].getAttributeValue("AXDisclosing"),
      0,
      "Row is not disclosing"
    );
    is(
      outRows[7]
        .getAttributeValue("AXDisclosedByRow")
        .getAttributeValue("AXDescription"),
      outRows[6].getAttributeValue("AXDescription"),
      "Row is direct child of row 6"
    );
    is(
      outRows[7].getAttributeValue("AXDisclosedRows").length,
      0,
      "Row has no row children"
    );
    is(
      outRows[7].getAttributeValue("AXDisclosureLevel"),
      2,
      "Row is level two"
    );
  }
);

// Test outline that isn't built with li/uls gets correct desc
addAccessibleTask(
  `
  <div role="tree" id="tree" tabindex="0" aria-label="My drive" aria-activedescendant="myfiles">
    <div id="myfiles" role="treeitem" aria-label="My files" aria-selected="true" aria-expanded="false">My files</div>
    <div role="treeitem" aria-label="Shared items" aria-selected="false" aria-expanded="false">Shared items</div>
  </div>
  `,
  async (browser, accDoc) => {
    const tree = getNativeInterface(accDoc, "tree");
    is(tree.getAttributeValue("AXRole"), "AXOutline", "Correct role for tree");

    const treeItems = tree.getAttributeValue("AXChildren");
    is(treeItems.length, 2, "Outline has two direct children");
    is(treeItems[0].getAttributeValue("AXSubrole"), "AXOutlineRow");
    is(treeItems[1].getAttributeValue("AXSubrole"), "AXOutlineRow");

    const outRows = tree.getAttributeValue("AXRows");
    is(outRows.length, 2, "Outline has two rows");

    is(
      outRows[0].getAttributeValue("AXDescription"),
      "My files",
      "files labelled correctly"
    );
    is(
      outRows[1].getAttributeValue("AXDescription"),
      "Shared items",
      "shared items labelled correctly"
    );
  }
);

// Test outline registers AXDisclosed attr as settable
addAccessibleTask(
  `
  <div role="tree" id="tree" tabindex="0" aria-label="My drive" aria-activedescendant="myfiles">
    <div id="myfiles" role="treeitem" aria-label="My files" aria-selected="true" aria-expanded="false">My files</div>
    <div role="treeitem" aria-label="Shared items" aria-selected="false" aria-expanded="true">Shared items</div>
  </div>
  `,
  async (browser, accDoc) => {
    const tree = getNativeInterface(accDoc, "tree");
    const treeItems = tree.getAttributeValue("AXChildren");

    is(treeItems.length, 2, "Outline has two direct children");
    is(treeItems[0].getAttributeValue("AXDisclosing"), 0);
    is(treeItems[1].getAttributeValue("AXDisclosing"), 1);

    is(treeItems[0].isAttributeSettable("AXDisclosing"), true);
    is(treeItems[1].isAttributeSettable("AXDisclosing"), true);

    // attempt to change attribute values
    treeItems[0].setAttributeValue("AXDisclosing", 1);
    treeItems[0].setAttributeValue("AXDisclosing", 0);

    // verify they're unchanged
    is(treeItems[0].getAttributeValue("AXDisclosing"), 0);
    is(treeItems[1].getAttributeValue("AXDisclosing"), 1);
  }
);
