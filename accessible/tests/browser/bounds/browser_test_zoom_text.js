/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

/* global getContentDPR */

async function runTests(browser, accDoc) {
  async function testTextNode(id) {
    let hyperTextNode = findAccessibleChildByID(accDoc, id);
    let textNode = hyperTextNode.firstChild;

    let contentDPR = await getContentDPR(browser);
    let [x, y, width, height] = getBounds(textNode, contentDPR);
    testTextBounds(hyperTextNode, 0, -1, [x, y, width, height],
                   COORDTYPE_SCREEN_RELATIVE);
  }

  async function testEmptyInputNode(id) {
    let inputNode = findAccessibleChildByID(accDoc, id);

    let [x, y, width, height] = getBounds(inputNode);
    testTextBounds(inputNode, 0, -1, [x, y, width, height],
                   COORDTYPE_SCREEN_RELATIVE);
    testTextBounds(inputNode, 0, 0, [x, y, width, height],
                   COORDTYPE_SCREEN_RELATIVE);
  }

  loadFrameScripts(browser, { name: "layout.js", dir: MOCHITESTS_DIR });

  await testTextNode("p1");
  await testTextNode("p2");
  await testEmptyInputNode("i1");

  await ContentTask.spawn(browser, {}, () => {
    zoomDocument(document, 2.0);
  });

  await testTextNode("p1");

  await ContentTask.spawn(browser, {}, () => {
    zoomDocument(document, 1.0);
  });
}

/**
 * Test the text range boundary when page is zoomed
 */
addAccessibleTask(`
  <p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2'>ل</p>
  <form><input id='i1' /></form>`, runTests
);
