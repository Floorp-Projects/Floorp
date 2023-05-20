/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

/**
 * Test nsIAccessibleText::scrollSubstringTo.
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
    pre {
      font: 20px/20px Ahem;
      height: 40px;
      overflow-y: scroll;
    }
  </style>
  <pre id="text">





It's a jetpack, Michael. What could possibly go wrong?





The only thing I found in the fridge was a dead dove in a bag.
</pre>`,
  async function (browser, docAcc) {
    let text = findAccessibleChildByID(docAcc, "text", [nsIAccessibleText]);
    let [, containerY, , containerHeight] = getBounds(text);
    let getCharY = () => {
      let objY = {};
      text.getCharacterExtents(7, {}, objY, {}, {}, COORDTYPE_SCREEN_RELATIVE);
      return objY.value;
    };
    ok(
      containerHeight < getCharY(),
      "Character is outside of container bounds"
    );
    text.scrollSubstringTo(7, 8, SCROLL_TYPE_TOP_EDGE);

    await waitForContentPaint(browser);
    await untilCacheIs(
      getCharY,
      containerY,
      "Character is scrolled to top of container"
    );
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);
