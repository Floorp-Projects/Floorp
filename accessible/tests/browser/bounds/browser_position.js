/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test changing the left/top CSS properties.
 */
addAccessibleTask(
  `
<div id="div" style="position: relative; left: 0px; top: 0px; width: fit-content;">
  test
</div>
  `,
  async function(browser, docAcc) {
    await testBoundsWithContent(docAcc, "div", browser);
    info("Changing left");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").style.left = "200px";
    });
    await waitForContentPaint(browser);
    await testBoundsWithContent(docAcc, "div", browser);
    info("Changing top");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("div").style.top = "200px";
    });
    await waitForContentPaint(browser);
    await testBoundsWithContent(docAcc, "div", browser);
  },
  { chrome: true, topLevel: true, iframe: true }
);
