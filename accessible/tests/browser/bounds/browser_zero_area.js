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
 * Ensure frames with zero area have their x, y coordinates correctly reported
 * in bounds()
 */
addAccessibleTask(
  `
<br>
<div id="a" style="height:0; width:0;"></div>
`,
  async function(browser, accDoc) {
    let a = findAccessibleChildByID(accDoc, "a");
    await testContentBounds(browser, a, 0, 0);
  }
);
