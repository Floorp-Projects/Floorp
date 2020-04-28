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
 * Test required and aria-required attributes on checkboxes
 * and radio buttons.
 */
addAccessibleTask(
  `
  <form>
    <input type="checkbox" id="checkbox" required>
   <br>
    <input type="radio" id="radio" required>
   <br>
    <input type="checkbox" id="ariaCheckbox" aria-required="true">
   <br>
    <input type="radio" id="ariaRadio" aria-required="true">
  </form>
  `,
  async (browser, accDoc) => {
    // Check initial AXRequired values are correct
    let radio = getNativeInterface(accDoc, "radio");
    is(
      radio.getAttributeValue("AXRequired"),
      1,
      "Correct required val for radio"
    );

    let ariaRadio = getNativeInterface(accDoc, "ariaRadio");
    is(
      ariaRadio.getAttributeValue("AXRequired"),
      1,
      "Correct required val for ariaRadio"
    );

    let checkbox = getNativeInterface(accDoc, "checkbox");
    is(
      checkbox.getAttributeValue("AXRequired"),
      1,
      "Correct required val for checkbox"
    );

    let ariaCheckbox = getNativeInterface(accDoc, "ariaCheckbox");
    is(
      ariaCheckbox.getAttributeValue("AXRequired"),
      1,
      "Correct required val for ariaCheckbox"
    );

    // Change aria-required, verify AXRequired is updated
    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "ariaCheckbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("ariaCheckbox")
        .setAttribute("aria-required", "false");
    });
    await stateChanged;

    is(
      ariaCheckbox.getAttributeValue("AXRequired"),
      0,
      "Correct required after false set for ariaCheckbox"
    );

    // Remove aria-required, verify AXRequired is updated
    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "ariaCheckbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("ariaCheckbox")
        .removeAttribute("aria-required");
    });
    await stateChanged;

    is(
      ariaCheckbox.getAttributeValue("AXRequired"),
      0,
      "Correct required after removal for ariaCheckbox"
    );

    // Change aria-required, verify AXRequired is updated
    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "ariaRadio");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("ariaRadio")
        .setAttribute("aria-required", "false");
    });
    await stateChanged;

    is(
      ariaRadio.getAttributeValue("AXRequired"),
      0,
      "Correct required after false set for ariaRadio"
    );

    // Remove aria-required, verify AXRequired is updated
    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "ariaRadio");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("ariaRadio")
        .removeAttribute("aria-required");
    });
    await stateChanged;

    is(
      ariaRadio.getAttributeValue("AXRequired"),
      0,
      "Correct required after removal for ariaRadio"
    );

    // Remove required, verify AXRequired is updated
    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "checkbox");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("checkbox").removeAttribute("required");
    });
    await stateChanged;

    is(
      checkbox.getAttributeValue("AXRequired"),
      0,
      "Correct required after removal for checkbox"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "radio");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("radio").removeAttribute("required");
    });
    await stateChanged;

    is(
      checkbox.getAttributeValue("AXRequired"),
      0,
      "Correct required after removal for radio"
    );
  }
);
