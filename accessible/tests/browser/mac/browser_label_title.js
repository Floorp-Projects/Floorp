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
 * Test different labeling/titling schemes for text fields
 */
addAccessibleTask(
  `<label for="n1">Label</label> <input id="n1">
   <label for="n2">Two</label> <label for="n2">Labels</label> <input id="n2">
   <input aria-label="ARIA Label" id="n3">`,
  (browser, accDoc) => {
    let n1 = getNativeInterface(accDoc, "n1");
    let n1Label = n1.getAttributeValue("AXTitleUIElement");
    // XXX: In Safari the label is an AXText with an AXValue,
    // here it is an AXGroup witth an AXTitle
    is(n1Label.getAttributeValue("AXTitle"), "Label");

    let n2 = getNativeInterface(accDoc, "n2");
    is(n2.getAttributeValue("AXDescription"), "TwoLabels");

    let n3 = getNativeInterface(accDoc, "n3");
    is(n3.getAttributeValue("AXDescription"), "ARIA Label");
  }
);

/**
 * Test to see that named groups get labels
 */
addAccessibleTask(
  `<fieldset id="fieldset"><legend>Fields</legend><input aria-label="hello"></fieldset>`,
  (browser, accDoc) => {
    let fieldset = getNativeInterface(accDoc, "fieldset");
    is(fieldset.getAttributeValue("AXDescription"), "Fields");
  }
);
