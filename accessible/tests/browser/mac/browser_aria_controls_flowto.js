/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test aria-controls
 */
addAccessibleTask(
  `<button aria-controls="info" id="info-button">Show info</button>
   <div id="info">Information.</div>
   <div id="more-info">More information.</div>`,
  async (browser, accDoc) => {
    const isARIAControls = (id, expectedIds) =>
      Assert.deepEqual(
        getNativeInterface(accDoc, id)
          .getAttributeValue("AXARIAControls")
          .map(e => e.getAttributeValue("AXDOMIdentifier")),
        expectedIds,
        `"${id}" has correct AXARIAControls`
      );

    isARIAControls("info-button", ["info"]);

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("info-button")
        .setAttribute("aria-controls", "info more-info");
    });

    isARIAControls("info-button", ["info", "more-info"]);
  }
);

/**
 * Test aria-flowto
 */
addAccessibleTask(
  `<button aria-flowto="info" id="info-button">Show info</button>
   <div id="info">Information.</div>
   <div id="more-info">More information.</div>`,
  async (browser, accDoc) => {
    const isLinkedUIElements = (id, expectedIds) =>
      Assert.deepEqual(
        getNativeInterface(accDoc, id)
          .getAttributeValue("AXLinkedUIElements")
          .map(e => e.getAttributeValue("AXDOMIdentifier")),
        expectedIds,
        `"${id}" has correct AXARIAControls`
      );

    isLinkedUIElements("info-button", ["info"]);

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("info-button")
        .setAttribute("aria-flowto", "info more-info");
    });

    isLinkedUIElements("info-button", ["info", "more-info"]);
  }
);

/**
 * Test aria-controls
 */
addAccessibleTask(
  `<input type="radio" id="cat-radio" name="animal"><label for="cat">Cat</label>
   <input type="radio" id="dog-radio" name="animal" aria-flowto="info"><label for="dog">Dog</label>
   <div id="info">Information.</div>`,
  async (browser, accDoc) => {
    const isLinkedUIElements = (id, expectedIds) =>
      Assert.deepEqual(
        getNativeInterface(accDoc, id)
          .getAttributeValue("AXLinkedUIElements")
          .map(e => e.getAttributeValue("AXDOMIdentifier")),
        expectedIds,
        `"${id}" has correct AXARIAControls`
      );

    isLinkedUIElements("dog-radio", ["cat-radio", "dog-radio", "info"]);
  }
);
