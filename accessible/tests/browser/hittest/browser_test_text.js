/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
a
<div id="noChars" style="width: 5px; height: 5px;"><p></p></div>
<p id="twoText"><span>a</span><span>b</span></p>
  `,
  async function(browser, docAcc) {
    const dpr = await getContentDPR(browser);
    // Test getOffsetAtPoint on a container containing no characters. The inner
    // container does not include the requested point, but the outer one does.
    const noChars = findAccessibleChildByID(docAcc, "noChars", [
      Ci.nsIAccessibleText,
    ]);
    let [x, y] = Layout.getBounds(noChars, dpr);
    await testOffsetAtPoint(noChars, x, y, COORDTYPE_SCREEN_RELATIVE, -1);

    // Test that the correct offset is returned for a point in a second text
    // leaf.
    const twoText = findAccessibleChildByID(docAcc, "twoText", [
      Ci.nsIAccessibleText,
    ]);
    const text2 = twoText.getChildAt(1);
    [x, y] = Layout.getBounds(text2, dpr);
    await testOffsetAtPoint(twoText, x, y, COORDTYPE_SCREEN_RELATIVE, 1);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);
