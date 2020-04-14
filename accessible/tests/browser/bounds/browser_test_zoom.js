/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function testContentBounds(browser, acc) {
  let [
    expectedX,
    expectedY,
    expectedWidth,
    expectedHeight,
  ] = await getContentBoundsForDOMElm(browser, getAccessibleDOMNodeID(acc));

  let contentDPR = await getContentDPR(browser);
  let [x, y, width, height] = getBounds(acc, contentDPR);
  let prettyAccName = prettyName(acc);
  is(x, expectedX, "Wrong x coordinate of " + prettyAccName);
  is(y, expectedY, "Wrong y coordinate of " + prettyAccName);
  is(width, expectedWidth, "Wrong width of " + prettyAccName);
  ok(height >= expectedHeight, "Wrong height of " + prettyAccName);
}

async function runTests(browser, accDoc) {
  let p1 = findAccessibleChildByID(accDoc, "p1");
  let p2 = findAccessibleChildByID(accDoc, "p2");
  let imgmap = findAccessibleChildByID(accDoc, "imgmap");
  if (!imgmap.childCount) {
    // An image map may not be available even after the doc and image load
    // is complete. We don't recieve any DOM events for this change either,
    // so we need to wait for a REORDER.
    await waitForEvent(EVENT_REORDER, "imgmap");
  }
  let area = imgmap.firstChild;

  await testContentBounds(browser, p1);
  await testContentBounds(browser, p2);
  await testContentBounds(browser, area);

  await invokeContentTask(browser, [], () => {
    const { Layout } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );
    Layout.zoomDocument(content.document, 2.0);
  });

  await testContentBounds(browser, p1);
  await testContentBounds(browser, p2);
  await testContentBounds(browser, area);
}

/**
 * Test accessible boundaries when page is zoomed
 */
addAccessibleTask(
  `
<p id="p1">para 1</p><p id="p2">para 2</p>
<map name="atoz_map" id="map">
  <area id="area1" href="http://mozilla.org"
        coords=17,0,30,14" alt="mozilla.org" shape="rect">
</map>
<img id="imgmap" width="447" height="15"
     usemap="#atoz_map"
     src="http://example.com/a11y/accessible/tests/mochitest/letters.gif">`,
  runTests,
  { iframe: true, remoteIframe: true }
);
