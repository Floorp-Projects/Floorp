/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function testContentBounds(browser, acc) {
  let [expectedX, expectedY, expectedWidth, expectedHeight] =
    await getContentBoundsForDOMElm(browser, getAccessibleDOMNodeID(acc));

  let contentDPR = await getContentDPR(browser);
  let [x, y, width, height] = getBounds(acc, contentDPR);
  let prettyAccName = prettyName(acc);
  is(x, expectedX, "Wrong x coordinate of " + prettyAccName);
  is(y, expectedY, "Wrong y coordinate of " + prettyAccName);
  is(width, expectedWidth, "Wrong width of " + prettyAccName);
  ok(height >= expectedHeight, "Wrong height of " + prettyAccName);
}

async function runTests(browser, accDoc) {
  let p = findAccessibleChildByID(accDoc, "div");
  let p2 = findAccessibleChildByID(accDoc, "p");

  await testContentBounds(browser, p);
  await testContentBounds(browser, p2);
}

/**
 * Test accessible bounds for accs with display:contents
 */
addAccessibleTask(
  `
  <div id="div">before
    <ul id="ul" style="display: contents;">
      <li id="li" style="display: contents;">
        <p id="p">item</p>
      </li>
    </ul>
  </div>`,
  runTests,
  { iframe: true, remoteIframe: true }
);
