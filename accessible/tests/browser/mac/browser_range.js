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
 * Verify that the value of a slider input can be incremented/decremented
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
 * Verify that the value of a slider input can be set directly
 * Test input[type=range]
 */
addAccessibleTask(
  `<input id="range" type="range" min="1" max="100" value="1" step="10">`,
  async (browser, accDoc) => {
    let nextValue = 21;
    let range = getNativeInterface(accDoc, "range");
    is(range.getAttributeValue("AXRole"), "AXSlider", "Correct AXSlider role");
    is(range.getAttributeValue("AXValue"), 1, "Correct initial value");

    ok(range.isAttributeSettable("AXValue"), "Range AXValue is settable.");

    let evt = waitForMacEvent("AXValueChanged");
    range.setAttributeValue("AXValue", nextValue);
    await evt;
    is(range.getAttributeValue("AXValue"), nextValue, "Correct updated value");
  }
);

/**
 * Verify that the value of a number input can be incremented/decremented
 * Test input[type=number]
 */
addAccessibleTask(
  `<input type="number" value="11" id="number" step=".05">`,
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
    is(number.getAttributeValue("AXValue"), 11.05, "Correct increment value");

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

/**
 * Test Min, Max, Orientation, ValueDescription
 */
addAccessibleTask(
  `<input type="number" value="11" id="number">`,
  async (browser, accDoc) => {
    let nextValue = 21;
    let number = getNativeInterface(accDoc, "number");
    is(
      number.getAttributeValue("AXRole"),
      "AXIncrementor",
      "Correct AXIncrementor role"
    );
    is(number.getAttributeValue("AXValue"), 11, "Correct initial value");

    ok(number.isAttributeSettable("AXValue"), "Range AXValue is settable.");

    let evt = waitForMacEvent("AXValueChanged");
    number.setAttributeValue("AXValue", nextValue);
    await evt;
    is(number.getAttributeValue("AXValue"), nextValue, "Correct updated value");
  }
);

/**
 * Verify that the value of a number input can be set directly
 * Test input[type=number]
 */
addAccessibleTask(
  `<div aria-valuetext="High" id="slider" aria-orientation="horizontal" role="slider" aria-valuenow="2" aria-valuemin="0" aria-valuemax="3"></div>`,
  async (browser, accDoc) => {
    let slider = getNativeInterface(accDoc, "slider");
    is(
      slider.getAttributeValue("AXValueDescription"),
      "High",
      "Correct value description"
    );
    is(
      slider.getAttributeValue("AXOrientation"),
      "AXHorizontalOrientation",
      "Correct orientation"
    );
    is(slider.getAttributeValue("AXMinValue"), 0, "Correct min value");
    is(slider.getAttributeValue("AXMaxValue"), 3, "Correct max value");

    let evt = waitForMacEvent("AXValueChanged");
    await invokeContentTask(browser, [], () => {
      const s = content.document.getElementById("slider");
      s.setAttribute("aria-valuetext", "Low");
    });
    await evt;
    is(
      slider.getAttributeValue("AXValueDescription"),
      "Low",
      "Correct value description"
    );

    evt = waitForEvent(EVENT_OBJECT_ATTRIBUTE_CHANGED, "slider");
    await invokeContentTask(browser, [], () => {
      const s = content.document.getElementById("slider");
      s.setAttribute("aria-orientation", "vertical");
      s.setAttribute("aria-valuemin", "-1");
      s.setAttribute("aria-valuemax", "5");
    });
    await evt;
    is(
      slider.getAttributeValue("AXOrientation"),
      "AXVerticalOrientation",
      "Correct orientation"
    );
    is(slider.getAttributeValue("AXMinValue"), -1, "Correct min value");
    is(slider.getAttributeValue("AXMaxValue"), 5, "Correct max value");
  }
);
