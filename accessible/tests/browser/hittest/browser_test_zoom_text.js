/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test if getOffsetAtPoint returns the given text offset at given coordinates.
 */
function testOffsetAtPoint(hyperText, x, y, coordType, expectedOffset) {
  is(
    hyperText.getOffsetAtPoint(x, y, coordType),
    expectedOffset,
    `Wrong offset at given point (${x}, ${y}) for ${prettyName(hyperText)}`
  );
}

async function runTests(browser, accDoc) {
  const expectedLength = await invokeContentTask(browser, [], () => {
    const { CommonUtils } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Common.jsm"
    );
    const hyperText = CommonUtils.getNode("paragraph", content.document);
    return Math.floor(hyperText.textContent.length / 2);
  });
  const hyperText = findAccessibleChildByID(accDoc, "paragraph", [
    Ci.nsIAccessibleText,
  ]);
  const textNode = hyperText.firstChild;

  let [x, y, width, height] = Layout.getBounds(
    textNode,
    await getContentDPR(browser)
  );

  testOffsetAtPoint(
    hyperText,
    x + width / 2,
    y + height / 2,
    COORDTYPE_SCREEN_RELATIVE,
    expectedLength
  );

  await invokeContentTask(browser, [], () => {
    const { Layout } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );

    Layout.zoomDocument(content.document, 2.0);
    content.document.body.offsetTop; // getBounds doesn't flush layout on its own.
  });

  [x, y, width, height] = Layout.getBounds(
    textNode,
    await getContentDPR(browser)
  );

  testOffsetAtPoint(
    hyperText,
    x + width / 2,
    y + height / 2,
    COORDTYPE_SCREEN_RELATIVE,
    expectedLength
  );
}

addAccessibleTask(
  `<p id="paragraph" style="font-family: monospace;">Болтали две сорокии</p>`,
  runTests,
  {
    iframe: true,
    remoteIframe: true,
    iframeAttrs: { style: "width: 600px; height: 600px;" },
  }
);
