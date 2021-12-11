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
 * Test input[type=checkbox]
 */
addAccessibleTask(
  `<input type="checkbox" id="vehicle"><label for="vehicle"> Bike</label>`,
  async (browser, accDoc) => {
    let checkbox = getNativeInterface(accDoc, "vehicle");
    is(checkbox.getAttributeValue("AXValue"), 0, "Correct initial value");

    let actions = checkbox.actionNames;
    ok(actions.includes("AXPress"), "Has press action");

    let evt = waitForMacEvent("AXValueChanged", "vehicle");
    checkbox.performAction("AXPress");
    await evt;
    is(checkbox.getAttributeValue("AXValue"), 1, "Correct checked value");

    evt = waitForMacEvent("AXValueChanged", "vehicle");
    checkbox.performAction("AXPress");
    await evt;
    is(checkbox.getAttributeValue("AXValue"), 0, "Correct checked value");
  }
);

/**
 * Test aria-pressed toggle buttons
 */
addAccessibleTask(
  `<button id="toggle" aria-pressed="false">toggle</button>`,
  async (browser, accDoc) => {
    // Set up a callback to change the toggle value
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("toggle").onclick = e => {
        let curVal = e.target.getAttribute("aria-pressed");
        let nextVal = curVal == "false" ? "true" : "false";
        e.target.setAttribute("aria-pressed", nextVal);
      };
    });

    let toggle = getNativeInterface(accDoc, "toggle");
    is(toggle.getAttributeValue("AXValue"), 0, "Correct initial value");

    let actions = toggle.actionNames;
    ok(actions.includes("AXPress"), "Has press action");

    let evt = waitForMacEvent("AXValueChanged", "toggle");
    toggle.performAction("AXPress");
    await evt;
    is(toggle.getAttributeValue("AXValue"), 1, "Correct checked value");

    evt = waitForMacEvent("AXValueChanged", "toggle");
    toggle.performAction("AXPress");
    await evt;
    is(toggle.getAttributeValue("AXValue"), 0, "Correct checked value");
  }
);

/**
 * Test aria-checked with tri state
 */
addAccessibleTask(
  `<button role="checkbox" id="checkbox" aria-checked="false">toggle</button>`,
  async (browser, accDoc) => {
    // Set up a callback to change the toggle value
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("checkbox").onclick = e => {
        const states = ["false", "true", "mixed"];
        let currState = e.target.getAttribute("aria-checked");
        let nextState = states[(states.indexOf(currState) + 1) % states.length];
        e.target.setAttribute("aria-checked", nextState);
      };
    });

    let checkbox = getNativeInterface(accDoc, "checkbox");
    is(checkbox.getAttributeValue("AXValue"), 0, "Correct initial value");

    let actions = checkbox.actionNames;
    ok(actions.includes("AXPress"), "Has press action");

    let evt = waitForMacEvent("AXValueChanged", "checkbox");
    checkbox.performAction("AXPress");
    await evt;
    is(checkbox.getAttributeValue("AXValue"), 1, "Correct checked value");

    evt = waitForMacEvent("AXValueChanged", "checkbox");
    checkbox.performAction("AXPress");
    await evt;
    is(checkbox.getAttributeValue("AXValue"), 2, "Correct checked value");
  }
);

/**
 * Test input[type=radio]
 */
addAccessibleTask(
  `<input type="radio" id="huey" name="drone" value="huey" checked>
   <label for="huey">Huey</label>
   <input type="radio" id="dewey" name="drone" value="dewey">
   <label for="dewey">Dewey</label>`,
  async (browser, accDoc) => {
    let huey = getNativeInterface(accDoc, "huey");
    is(huey.getAttributeValue("AXValue"), 1, "Correct initial value for huey");

    let dewey = getNativeInterface(accDoc, "dewey");
    is(
      dewey.getAttributeValue("AXValue"),
      0,
      "Correct initial value for dewey"
    );

    let actions = dewey.actionNames;
    ok(actions.includes("AXPress"), "Has press action");

    let evt = Promise.all([
      waitForMacEvent("AXValueChanged", "huey"),
      waitForMacEvent("AXValueChanged", "dewey"),
    ]);
    dewey.performAction("AXPress");
    await evt;
    is(
      dewey.getAttributeValue("AXValue"),
      1,
      "Correct checked value for dewey"
    );
    is(huey.getAttributeValue("AXValue"), 0, "Correct checked value for huey");
  }
);

/**
 * Test role=switch
 */
addAccessibleTask(
  `<div role="switch" aria-checked="false" id="sw">hello</div>`,
  async (browser, accDoc) => {
    let sw = getNativeInterface(accDoc, "sw");
    is(sw.getAttributeValue("AXValue"), 0, "Initially, switch is off");
    is(sw.getAttributeValue("AXRole"), "AXCheckBox", "Has correct role");
    is(sw.getAttributeValue("AXSubrole"), "AXSwitch", "Has correct subrole");

    let stateChanged = Promise.all([
      waitForMacEvent("AXValueChanged", "sw"),
      waitForStateChange("sw", STATE_CHECKED, true),
    ]);

    // We should get a state change event, and a value change.
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("sw")
        .setAttribute("aria-checked", "true");
    });

    await stateChanged;

    is(sw.getAttributeValue("AXValue"), 1, "Switch is now on");
  }
);
