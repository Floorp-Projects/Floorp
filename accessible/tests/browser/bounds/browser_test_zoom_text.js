/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function runTests(browser, accDoc) {
  async function testTextNode(id) {
    let hyperTextNode = findAccessibleChildByID(accDoc, id);
    let textNode = hyperTextNode.firstChild;

    let contentDPR = await getContentDPR(browser);
    let [x, y, width, height] = getBounds(textNode, contentDPR);
    testTextBounds(
      hyperTextNode,
      0,
      -1,
      [x, y, width, height],
      COORDTYPE_SCREEN_RELATIVE
    );
    // A 0 range should return an empty rect.
    testTextBounds(
      hyperTextNode,
      0,
      0,
      [0, 0, 0, 0],
      COORDTYPE_SCREEN_RELATIVE
    );
  }

  async function testEmptyInputNode(id) {
    let inputNode = findAccessibleChildByID(accDoc, id);

    let [x, y, width, height] = getBounds(inputNode);
    testTextBounds(
      inputNode,
      0,
      -1,
      [x, y, width, height],
      COORDTYPE_SCREEN_RELATIVE
    );
    // A 0 range in an empty input should still return
    // rect of input node.
    testTextBounds(
      inputNode,
      0,
      0,
      [x, y, width, height],
      COORDTYPE_SCREEN_RELATIVE
    );
  }

  await testTextNode("p1");
  await testTextNode("p2");
  await testEmptyInputNode("i1");

  await SpecialPowers.spawn(browser, [], () => {
    const { Layout } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );
    Layout.zoomDocument(content.document, 2.0);
  });

  await testTextNode("p1");

  await SpecialPowers.spawn(browser, [], () => {
    const { Layout } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );
    Layout.zoomDocument(content.document, 1.0);
  });
}

/**
 * Test the text range boundary when page is zoomed
 */
addAccessibleTask(
  `
  <p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2'>ل</p>
  <form><input id='i1' /></form>`,
  runTests,
  { iframe: true, remoteIframe: true }
);
