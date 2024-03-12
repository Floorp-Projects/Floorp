/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test that inputs with placeholder text expose it via AXPlaceholderValue and
 * correctly _avoid_ exposing it via AXTItle, AXValue, or AXDescription.
 */
addAccessibleTask(
  `
  <input id="input" placeholder="Name"><br>
  <input id="input2" placeholder="Name" value="Elmer Fudd">
  `,
  (browser, accDoc) => {
    let input = getNativeInterface(accDoc, "input");
    let input2 = getNativeInterface(accDoc, "input2");

    is(
      input.getAttributeValue("AXPlaceholderValue"),
      "Name",
      "Correct placeholder value"
    );
    is(input.getAttributeValue("AXDescription"), "", "Correct label");
    is(input.getAttributeValue("AXTitle"), "", "Correct title");
    is(input.getAttributeValue("AXValue"), "", "Correct value");

    is(
      input2.getAttributeValue("AXPlaceholderValue"),
      "Name",
      "Correct placeholder value in presence of value"
    );
    is(input2.getAttributeValue("AXDescription"), "", "Correct label");
    is(input2.getAttributeValue("AXTitle"), "", "Correct title");
    is(input2.getAttributeValue("AXValue"), "Elmer Fudd", "Correct value");
  }
);

/**
 * Test that aria-placeholder gets exposed via AXPlaceholderValue and correctly
 * contributes to AXValue (but not title or description).
 */
addAccessibleTask(
  `
  <span id="date-of-birth">Birthday</span>
  <div
    id="bday"
    contenteditable
    role="textbox"
    aria-labelledby="date-of-birth"
    aria-placeholder="MM-DD-YYYY">
    MM-DD-YYYY
  </div>
  `,
  (browser, accDoc) => {
    let bday = getNativeInterface(accDoc, "bday");

    is(
      bday.getAttributeValue("AXPlaceholderValue"),
      "MM-DD-YYYY",
      "Correct placeholder value"
    );
    is(bday.getAttributeValue("AXDescription"), "Birthday", "Correct label");
    is(bday.getAttributeValue("AXTitle"), "", "Correct title");
    is(bday.getAttributeValue("AXValue"), "MM-DD-YYYY", "Correct value");
  }
);
