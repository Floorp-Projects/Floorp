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
    const getAriaControls = id =>
      JSON.stringify(
        getNativeInterface(accDoc, id)
          .getAttributeValue("AXARIAControls")
          .map(e => e.getAttributeValue("AXDOMIdentifier"))
      );

    await untilCacheIs(
      () => getAriaControls("info-button"),
      JSON.stringify(["info"]),
      "Info-button has correct initial controls"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("info-button")
        .setAttribute("aria-controls", "info more-info");
    });

    await untilCacheIs(
      () => getAriaControls("info-button"),
      JSON.stringify(["info", "more-info"]),
      "Info-button has correct controls after mutation"
    );
  }
);

function getLinkedUIElements(accDoc, id) {
  return JSON.stringify(
    getNativeInterface(accDoc, id)
      .getAttributeValue("AXLinkedUIElements")
      .map(e => e.getAttributeValue("AXDOMIdentifier"))
  );
}

/**
 * Test aria-flowto
 */
addAccessibleTask(
  `<button aria-flowto="info" id="info-button">Show info</button>
   <div id="info">Information.</div>
   <div id="more-info">More information.</div>`,
  async (browser, accDoc) => {
    await untilCacheIs(
      () => getLinkedUIElements(accDoc, "info-button"),
      JSON.stringify(["info"]),
      "Info-button has correct initial linked elements"
    );

    await SpecialPowers.spawn(browser, [], () => {
      content.document
        .getElementById("info-button")
        .setAttribute("aria-flowto", "info more-info");
    });

    await untilCacheIs(
      () => getLinkedUIElements(accDoc, "info-button"),
      JSON.stringify(["info", "more-info"]),
      "Info-button has correct linked elements after mutation"
    );
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
    await untilCacheIs(
      () => getLinkedUIElements(accDoc, "dog-radio"),
      JSON.stringify(["cat-radio", "dog-radio", "info"]),
      "dog-radio has correct linked elements"
    );
  }
);
