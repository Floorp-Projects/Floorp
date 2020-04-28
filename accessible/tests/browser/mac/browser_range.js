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

/**
 * Test input[type=range]
 */
addAccessibleTask(
  `<input id="range" type="range" min="1" max="100" value="1" step="10">`,
  async (browser, accDoc) => {
    let range = getNativeInterface(accDoc, "range");
    is(range.getAttributeValue("AXRole"), "AXSlider", "Correct AXSlider role");
    is(range.getAttributeValue("AXValue"), 1, "Correct initial value");

    let actions = range.actionNames;
    ok(actions.includes("AXDecrement"), "Has decrement action");
    ok(actions.includes("AXIncrement"), "Has increment action");

    let evt = waitForMacEvent("AXValueChanged");
    range.performAction("AXIncrement");
    await evt;
    is(range.getAttributeValue("AXValue"), 11, "Correct increment value");

    evt = waitForMacEvent("AXValueChanged");
    range.performAction("AXDecrement");
    await evt;
    is(range.getAttributeValue("AXValue"), 1, "Correct decrement value");

    evt = waitForMacEvent("AXValueChanged");
    // Adjust value via script in content
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("range").value = 41;
    });
    await evt;
    is(
      range.getAttributeValue("AXValue"),
      41,
      "Correct value from content change"
    );
  }
);

/**
 * Test input[type=range]
 */
addAccessibleTask(
  `<input type="number" value="11" id="number">`,
  async (browser, accDoc) => {
    let number = getNativeInterface(accDoc, "number");
    is(
      number.getAttributeValue("AXRole"),
      "AXIncrementor",
      "Correct AXIncrementor role"
    );
    is(number.getAttributeValue("AXValue"), 11, "Correct initial value");

    let actions = number.actionNames;
    ok(actions.includes("AXDecrement"), "Has decrement action");
    ok(actions.includes("AXIncrement"), "Has increment action");

    let evt = waitForMacEvent("AXValueChanged");
    number.performAction("AXIncrement");
    await evt;
    is(number.getAttributeValue("AXValue"), 12, "Correct increment value");

    evt = waitForMacEvent("AXValueChanged");
    number.performAction("AXDecrement");
    await evt;
    is(number.getAttributeValue("AXValue"), 11, "Correct decrement value");

    evt = waitForMacEvent("AXValueChanged");
    // Adjust value via script in content
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("number").value = 42;
    });
    await evt;
    is(
      number.getAttributeValue("AXValue"),
      42,
      "Correct value from content change"
    );
  }
);
