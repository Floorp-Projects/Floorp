/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
loadScripts({ name: "states.js", dir: MOCHITESTS_DIR });

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

// Test outline rows correctly expose checkable, checked/unchecked/mixed status
addAccessibleTask(
  `
  <div role="tree" id="tree">
    <div role="treeitem" aria-checked="false" id="l1">
      Leaf 1
    </div>
    <div role="treeitem" aria-checked="true" id="l2">
      Leaf 2
    </div>
    <div role="treeitem" id="l3">
      Leaf 3
    </div>
    <div role="treeitem" aria-checked="mixed" id="l4">
      Leaf 4
    </div>
  </div>

  `,
  async (browser, accDoc) => {
    const tree = getNativeInterface(accDoc, "tree");
    const treeItems = tree.getAttributeValue("AXChildren");

    is(treeItems.length, 4, "Outline has four direct children");
    is(
      treeItems[0].getAttributeValue("AXValue"),
      0,
      "Child one is not checked"
    );
    is(treeItems[1].getAttributeValue("AXValue"), 1, "Child two is checked");
    is(
      treeItems[2].getAttributeValue("AXValue"),
      null,
      "Child three is not checkable and has no val"
    );
    is(treeItems[3].getAttributeValue("AXValue"), 2, "Child four is mixed");

    let stateChanged = Promise.all([
      waitForMacEvent("AXValueChanged", "l1"),
      waitForStateChange("l1", STATE_CHECKED, true),
    ]);
    // We should get a state change event for checked.
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("l1")
        .setAttribute("aria-checked", "true");
    });
    await stateChanged;
    is(treeItems[0].getAttributeValue("AXValue"), 1, "Child one is checked");

    stateChanged = Promise.all([
      waitForMacEvent("AXValueChanged", "l2"),
      waitForMacEvent("AXValueChanged", "l2"),
      waitForStateChange("l2", STATE_CHECKED, false),
      waitForStateChange("l2", STATE_CHECKABLE, false),
    ]);
    // We should get a state change event for both checked and checkable,
    // and value changes for both.
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("l2").removeAttribute("aria-checked");
    });
    await stateChanged;
    is(
      treeItems[1].getAttributeValue("AXValue"),
      null,
      "Child two is not checkable and has no val"
    );

    stateChanged = Promise.all([
      waitForMacEvent("AXValueChanged", "l3"),
      waitForMacEvent("AXValueChanged", "l3"),
      waitForStateChange("l3", STATE_CHECKED, true),
      waitForStateChange("l3", STATE_CHECKABLE, true),
    ]);
    // We should get a state change event for both checked and checkable,
    // and value changes for each.
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("l3")
        .setAttribute("aria-checked", "true");
    });
    await stateChanged;
    is(treeItems[2].getAttributeValue("AXValue"), 1, "Child three is checked");

    stateChanged = Promise.all([
      waitForMacEvent("AXValueChanged", "l4"),
      waitForMacEvent("AXValueChanged", "l4"),
      waitForStateChange("l4", STATE_MIXED, false),
      waitForStateChange("l4", STATE_CHECKABLE, false),
    ]);
    // We should get a state change event for both mixed and checkable,
    // and value changes for each.
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("l4").removeAttribute("aria-checked");
    });
    await stateChanged;
    is(
      treeItems[3].getAttributeValue("AXValue"),
      null,
      "Child four is not checkable and has no value"
    );
  }
);
