/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test focusability
 */
addAccessibleTask(
  `
  <div role="button" id="ariabutton">hello</div> <button id="button">world</button>
  `,
  async (browser, accDoc) => {
    let ariabutton = getNativeInterface(accDoc, "ariabutton");
    let button = getNativeInterface(accDoc, "button");

    is(
      ariabutton.getAttributeValue("AXFocused"),
      0,
      "aria button is not focused"
    );

    is(button.getAttributeValue("AXFocused"), 0, "button is not focused");

    ok(
      !ariabutton.isAttributeSettable("AXFocused"),
      "aria button should not be focusable"
    );

    ok(button.isAttributeSettable("AXFocused"), "button is focusable");

    let evt = waitForMacEvent(
      "AXFocusedUIElementChanged",
      iface => iface.getAttributeValue("AXDOMIdentifier") == "button"
    );

    button.setAttributeValue("AXFocused", true);

    await evt;

    is(button.getAttributeValue("AXFocused"), 1, "button is focused");
  }
);
