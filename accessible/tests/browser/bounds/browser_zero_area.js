/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function testContentBounds(browser, acc, expectedWidth, expectedHeight) {
  let [expectedX, expectedY] = await getContentBoundsForDOMElm(
    browser,
    getAccessibleDOMNodeID(acc)
  );

  let contentDPR = await getContentDPR(browser);
  let [x, y, width, height] = getBounds(acc, contentDPR);
  let prettyAccName = prettyName(acc);
  is(x, expectedX, "Wrong x coordinate of " + prettyAccName);
  is(y, expectedY, "Wrong y coordinate of " + prettyAccName);
  is(width, expectedWidth, "Wrong width of " + prettyAccName);
  is(height, expectedHeight, "Wrong height of " + prettyAccName);
}
/**
 * Test accessible bounds with different combinations of overflow and
 * non-zero frame area.
 */
addAccessibleTask(
  `
  <div id="a1" style="height:100px; width:100px; background:green;"></div>
  <div id="a2" style="height:100px; width:100px; background:green;"><div style="height:300px; max-width: 300px; background:blue;"></div></div>
  <div id="a3" style="height:0; width:0;"><div style="height:200px; width:200px; background:green;"></div></div>
  `,
  async function(browser, accDoc) {
    const a1 = findAccessibleChildByID(accDoc, "a1");
    const a2 = findAccessibleChildByID(accDoc, "a2");
    const a3 = findAccessibleChildByID(accDoc, "a3");
    await testContentBounds(browser, a1, 100, 100);
    await testContentBounds(browser, a2, 100, 100);
    await testContentBounds(browser, a3, 200, 200);
  }
);

/**
 * Ensure frames with zero area have their x, y coordinates correctly reported
 * in bounds()
 */
addAccessibleTask(
  `
<br>
<div id="a" style="height:0; width:0;"></div>
`,
  async function(browser, accDoc) {
    const a = findAccessibleChildByID(accDoc, "a");
    await testContentBounds(browser, a, 0, 0);
  }
);

/**
 * Ensure accessibles have accurately signed dimensions and position when
 * offscreen.
 */
addAccessibleTask(
  `
<input type="radio" id="radio" style="left: -671091em; position: absolute;">
`,
  async function(browser, accDoc) {
    const radio = findAccessibleChildByID(accDoc, "radio");
    const contentDPR = await getContentDPR(browser);
    const [x, y, width, height] = getBounds(radio, contentDPR);
    ok(x < 0, "X coordinate should be negative");
    ok(y > 0, "Y coordinate should be positive");
    ok(width > 0, "Width should be positive");
    ok(height > 0, "Height should be positive");
    // Note: the exact values of x, y, width, and height
    // are inconsistent with the DOM element values of those
    // fields, so we don't check our bounds against them with
    // `testContentBounds` here. DOM reports a negative width,
    // positive height, and a slightly different (+/- 20)
    // x and y.
  }
);
