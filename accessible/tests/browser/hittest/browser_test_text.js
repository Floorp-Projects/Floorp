/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
a
<div id="noChars" style="width: 5px; height: 5px;"><p></p></div>
<p id="twoText"><span>a</span><span>b</span></p>
<div id="iframeAtEnd" style="width: 20px; height: 20px;">
  a
  <iframe width="1" height="1"></iframe>
</div>
<button id="pointBeforeText">
  <div style="display: flex;">
    <div style="width: 100px; background-color: red;" role="none"></div>
    test
    <div style="width: 100px; background-color: blue;" role="none"></div>
  </div>
</button>
  `,
  async function (browser, docAcc) {
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

    // Test offsetAtPoint when there is an iframe at the end of the container.
    const iframeAtEnd = findAccessibleChildByID(docAcc, "iframeAtEnd", [
      Ci.nsIAccessibleText,
    ]);
    let width;
    let height;
    [x, y, width, height] = Layout.getBounds(iframeAtEnd, dpr);
    x += width - 1;
    y += height - 1;
    await testOffsetAtPoint(iframeAtEnd, x, y, COORDTYPE_SCREEN_RELATIVE, -1);

    // Test that 0 is returned if the point is within the container but before
    // the rectangle at offset 0. This is buggy behavior that some users have
    // unfortunately come to rely on (bug 1816601).
    const pointBeforeText = findAccessibleChildByID(docAcc, "pointBeforeText", [
      Ci.nsIAccessibleText,
    ]);
    [x, y, width, height] = Layout.getBounds(pointBeforeText, dpr);
    await testOffsetAtPoint(
      pointBeforeText,
      x + 1,
      y + 1,
      COORDTYPE_SCREEN_RELATIVE,
      0
    );
    // But this buggy behavior only applies for a point before offset 0, not
    // a point after the last offset. So it's asymmetrically buggy. :(
    await testOffsetAtPoint(
      pointBeforeText,
      x + width - 1,
      y + height - 1,
      COORDTYPE_SCREEN_RELATIVE,
      -1
    );
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);
