/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function runTests(browser, accDoc) {
  function testTextNode(id) {
    let hyperTextNode = findAccessibleChildByID(accDoc, id);
    let textNode = hyperTextNode.firstChild;

    let [x, y, width, height] = getBounds(textNode);
    testTextBounds(hyperTextNode, 0, -1, [x, y, width, height],
                   COORDTYPE_SCREEN_RELATIVE);
  }

  loadFrameScripts(browser, { name: 'layout.js', dir: MOCHITESTS_DIR });

  testTextNode("p1");
  testTextNode("p2");

  await ContentTask.spawn(browser, {}, () => {
    zoomDocument(document, 2.0);
  });

  testTextNode("p1");

  await ContentTask.spawn(browser, {}, () => {
    zoomDocument(document, 1.0);
  });
}

/**
 * Test the text range boundary when page is zoomed
 */
addAccessibleTask(`
  <p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2'>ل</p>`, runTests
);
