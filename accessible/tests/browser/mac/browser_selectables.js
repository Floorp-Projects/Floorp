/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

function getSelectedIds(selectable) {
  return selectable
    .getAttributeValue("AXSelectedChildren")
    .map(c => c.getAttributeValue("AXDOMIdentifier"));
}

/**
 * Test aria tabs
 */
addAccessibleTask("mac/doc_aria_tabs.html", async (browser, accDoc) => {
  let tablist = getNativeInterface(accDoc, "tablist");
  is(
    tablist.getAttributeValue("AXRole"),
    "AXTabGroup",
    "Correct role for tablist"
  );

  let tabMacAccs = tablist.getAttributeValue("AXTabs");
  is(tabMacAccs.length, 3, "3 items in AXTabs");

  let selectedTabs = tablist.getAttributeValue("AXSelectedChildren");
  is(selectedTabs.length, 1, "one selected tab");

  let tab = selectedTabs[0];
  is(tab.getAttributeValue("AXRole"), "AXRadioButton", "Correct role for tab");
  is(
    tab.getAttributeValue("AXSubrole"),
    "AXTabButton",
    "Correct subrole for tab"
  );
  is(tab.getAttributeValue("AXTitle"), "First Tab", "Correct title for tab");

  let tabToSelect = tabMacAccs[1];
  is(
    tabToSelect.getAttributeValue("AXTitle"),
    "Second Tab",
    "Correct title for tab"
  );

  let actions = tabToSelect.actionNames;
  ok(true, actions);
  ok(actions.includes("AXPress"), "Has switch action");

  let evt = waitForMacEvent("AXSelectedChildrenChanged");
  tabToSelect.performAction("AXPress");
  await evt;

  selectedTabs = tablist.getAttributeValue("AXSelectedChildren");
  is(selectedTabs.length, 1, "one selected tab");
  is(
    selectedTabs[0].getAttributeValue("AXTitle"),
    "Second Tab",
    "Correct title for tab"
  );
});

addAccessibleTask('<p id="p">hello</p>', async (browser, accDoc) => {
  let p = getNativeInterface(accDoc, "p");
  ok(
    p.attributeNames.includes("AXSelected"),
    "html element includes 'AXSelected' attribute"
  );
  is(p.getAttributeValue("AXSelected"), 0, "AX selected is 'false'");
});

addAccessibleTask(
  `<select id="select" aria-label="Choose a number" multiple>
    <option id="one" selected>One</option>
    <option id="two">Two</option>
    <option id="three">Three</option>
    <option id="four" disabled>Four</option>
  </select>`,
  async (browser, accDoc) => {
    let select = getNativeInterface(accDoc, "select");
    let one = getNativeInterface(accDoc, "one");
    let two = getNativeInterface(accDoc, "two");
    let three = getNativeInterface(accDoc, "three");
    let four = getNativeInterface(accDoc, "four");

    is(
      select.getAttributeValue("AXTitle"),
      "Choose a number",
      "Select titled correctly"
    );
    ok(
      select.attributeNames.includes("AXOrientation"),
      "Have orientation attribute"
    );
    ok(
      select.isAttributeSettable("AXSelectedChildren"),
      "Select can have AXSelectedChildren set"
    );

    is(one.getAttributeValue("AXTitle"), "", "Option should not have a title");
    is(
      one.getAttributeValue("AXValue"),
      "One",
      "Option should have correct value"
    );
    is(
      one.getAttributeValue("AXRole"),
      "AXStaticText",
      "Options should have AXStaticText role"
    );
    ok(one.isAttributeSettable("AXSelected"), "Option can have AXSelected set");

    is(select.getAttributeValue("AXSelectedChildren").length, 1);
    let evt = waitForMacEvent("AXSelectedChildrenChanged");
    one.setAttributeValue("AXSelected", false);
    await evt;
    is(select.getAttributeValue("AXSelectedChildren").length, 0);
    evt = waitForMacEvent("AXSelectedChildrenChanged");
    three.setAttributeValue("AXSelected", true);
    await evt;
    is(select.getAttributeValue("AXSelectedChildren").length, 1);
    ok(getSelectedIds(select).includes("three"), "'three' is selected");
    evt = waitForMacEvent("AXSelectedChildrenChanged");
    select.setAttributeValue("AXSelectedChildren", [one, two]);
    await evt;
    await untilCacheOk(() => {
      let ids = getSelectedIds(select);
      return ids[0] == "one" && ids[1] == "two";
    }, "Got correct selected children");

    evt = waitForMacEvent("AXSelectedChildrenChanged");
    select.setAttributeValue("AXSelectedChildren", [three, two, four]);
    await evt;
    await untilCacheOk(() => {
      let ids = getSelectedIds(select);
      return ids[0] == "two" && ids[1] == "three";
    }, "Got correct selected children");

    ok(!four.getAttributeValue("AXEnabled"), "Disabled option is disabled");
  }
);

addAccessibleTask(
  `<select id="select" aria-label="Choose a thing" multiple>
    <optgroup label="Fruits">
      <option id="banana" selected>Banana</option>
      <option id="apple">Apple</option>
      <option id="orange">Orange</option>
    </optgroup>
    <optgroup label="Vegetables">
      <option id="lettuce" selected>Lettuce</option>
      <option id="tomato">Tomato</option>
      <option id="onion">Onion</option>
    </optgroup>
    <optgroup label="Spices">
      <option id="cumin">Cumin</option>
      <option id="coriander">Coriander</option>
      <option id="allspice" selected>Allspice</option>
    </optgroup>
    <option id="everything">Everything</option>
  </select>`,
  async (browser, accDoc) => {
    let select = getNativeInterface(accDoc, "select");

    is(
      select.getAttributeValue("AXTitle"),
      "Choose a thing",
      "Select titled correctly"
    );
    ok(
      select.attributeNames.includes("AXOrientation"),
      "Have orientation attribute"
    );
    ok(
      select.isAttributeSettable("AXSelectedChildren"),
      "Select can have AXSelectedChildren set"
    );
    let childValueSelectablePairs = select
      .getAttributeValue("AXChildren")
      .map(c => [
        c.getAttributeValue("AXValue"),
        c.isAttributeSettable("AXSelected"),
        c.getAttributeValue("AXEnabled"),
      ]);
    [
      ["​", false, 0],
      ["Fruits", false, 0],
      ["Banana", true, 1],
      ["Apple", true, 1],
      ["Orange", true, 1],
      ["​", false, 0],
      ["Vegetables", false, 0],
      ["Lettuce", true, 1],
      ["Tomato", true, 1],
      ["Onion", true, 1],
      ["​", false, 0],
      ["Spices", false, 0],
      ["Cumin", true, 1],
      ["Coriander", true, 1],
      ["Allspice", true, 1],
      ["Everything", true, 1],
    ];
    Assert.deepEqual(
      childValueSelectablePairs,
      [
        ["​", false, false],
        ["Fruits", false, false],
        ["Banana", true, true],
        ["Apple", true, true],
        ["Orange", true, true],
        ["​", false, false],
        ["Vegetables", false, false],
        ["Lettuce", true, true],
        ["Tomato", true, true],
        ["Onion", true, true],
        ["​", false, false],
        ["Spices", false, false],
        ["Cumin", true, true],
        ["Coriander", true, true],
        ["Allspice", true, true],
        ["Everything", true, true],
      ],
      "Options are selectable, group labels are not"
    );

    let allspice = getNativeInterface(accDoc, "allspice");
    is(
      allspice.getAttributeValue("AXTitle"),
      "",
      "Option should not have a title"
    );
    is(
      allspice.getAttributeValue("AXValue"),
      "Allspice",
      "Option should have a value"
    );
    is(
      allspice.getAttributeValue("AXRole"),
      "AXStaticText",
      "Options should have AXStaticText role"
    );
    ok(
      allspice.isAttributeSettable("AXSelected"),
      "Option can have AXSelected set"
    );
    is(
      allspice
        .getAttributeValue("AXParent")
        .getAttributeValue("AXDOMIdentifier"),
      "select",
      "Select is direct parent of nested option"
    );

    let groupLabel = select.getAttributeValue("AXChildren")[1];
    ok(
      !groupLabel.isAttributeSettable("AXSelected"),
      "Group label should not be selectable"
    );
    is(
      groupLabel.getAttributeValue("AXValue"),
      "Fruits",
      "Group label should have a value"
    );
    is(
      groupLabel.getAttributeValue("AXTitle"),
      null,
      "Group label should not have a title"
    );
    is(
      groupLabel.getAttributeValue("AXRole"),
      "AXStaticText",
      "Group label should have AXStaticText role"
    );
    is(
      groupLabel
        .getAttributeValue("AXParent")
        .getAttributeValue("AXDOMIdentifier"),
      "select",
      "Select is direct parent of group label"
    );

    Assert.deepEqual(getSelectedIds(select), ["banana", "lettuce", "allspice"]);
  }
);

addAccessibleTask(
  `<div role="listbox" id="select" aria-label="Choose a number" aria-multiselectable="true">
    <div role="option" id="one" aria-selected="true">One</div>
    <div role="option" id="two">Two</div>
    <div role="option" id="three">Three</div>
    <div role="option" id="four" aria-disabled="true">Four</div>
</div>`,
  async (browser, accDoc) => {
    let select = getNativeInterface(accDoc, "select");
    let one = getNativeInterface(accDoc, "one");

    is(
      select.getAttributeValue("AXTitle"),
      "Choose a number",
      "Select titled correctly"
    );
    ok(
      select.attributeNames.includes("AXOrientation"),
      "Have orientation attribute"
    );
    ok(
      select.isAttributeSettable("AXSelectedChildren"),
      "Select can have AXSelectedChildren set"
    );

    is(one.getAttributeValue("AXTitle"), "", "Option should not have a title");
    is(
      one.getAttributeValue("AXValue"),
      "One",
      "Option should have correct value"
    );
    is(
      one.getAttributeValue("AXRole"),
      "AXStaticText",
      "Options should have AXStaticText role"
    );
    ok(one.isAttributeSettable("AXSelected"), "Option can have AXSelected set");

    is(select.getAttributeValue("AXSelectedChildren").length, 1);
    let evt = waitForMacEvent("AXSelectedChildrenChanged");
    // Change selection from content.
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("one").removeAttribute("aria-selected");
    });
    await evt;
    is(select.getAttributeValue("AXSelectedChildren").length, 0);
  }
);
