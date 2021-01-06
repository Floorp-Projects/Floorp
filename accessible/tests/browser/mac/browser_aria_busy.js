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
 * Test aria-busy
 */
addAccessibleTask(
  `<div id="section" role="group">Hello</div>`,
  async (browser, accDoc) => {
    let section = getNativeInterface(accDoc, "section");

    ok(!section.getAttributeValue("AXElementBusy"), "section is not busy");

    let busyChanged = waitForMacEvent("AXElementBusyChanged", "section");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("section")
        .setAttribute("aria-busy", "true");
    });
    await busyChanged;

    ok(section.getAttributeValue("AXElementBusy"), "section is busy");

    busyChanged = waitForMacEvent("AXElementBusyChanged", "section");
    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("section")
        .setAttribute("aria-busy", "false");
    });
    await busyChanged;

    ok(!section.getAttributeValue("AXElementBusy"), "section is not busy");
  }
);
