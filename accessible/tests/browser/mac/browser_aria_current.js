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
 * Test aria-current
 */
addAccessibleTask(
  `<a id="one" href="%23" aria-current="page">One</a><a id="two" href="%23">Two</a>`,
  async (browser, accDoc) => {
    let one = getNativeInterface(accDoc, "one");
    let two = getNativeInterface(accDoc, "two");

    is(
      one.getAttributeValue("AXARIACurrent"),
      "page",
      "Correct aria-current for #one"
    );
    is(
      two.getAttributeValue("AXARIACurrent"),
      null,
      "Correct aria-current for #two"
    );

    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "one");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("one")
        .setAttribute("aria-current", "step");
    });
    await stateChanged;

    is(
      one.getAttributeValue("AXARIACurrent"),
      "step",
      "Correct aria-current for #one"
    );

    stateChanged = waitForEvent(EVENT_STATE_CHANGE, "one");
    await SpecialPowers.spawn(browser, [], () => {
      content.document.getElementById("one").removeAttribute("aria-current");
    });
    await stateChanged;

    is(
      one.getAttributeValue("AXARIACurrent"),
      null,
      "Correct aria-current for #one"
    );
  }
);
