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

/**
 * Test moving one element by inserting a second element before it such that the
 * second element doesn't reflow.
 */
addAccessibleTask(
  `
<div id="reflowContainer">
  <p>1</p>
  <p id="reflow2" hidden>2</p>
  <p id="reflow3">3</p>
</div>
<p id="noReflow">noReflow</p>
  `,
  async function(browser, docAcc) {
    for (const id of ["reflowContainer", "reflow3", "noReflow"]) {
      await testBoundsWithContent(docAcc, id, browser);
    }
    // Show p2, which will reflow everything inside "reflowContainer", but just
    // move "noReflow" down without reflowing it.
    info("Showing p2");
    let shown = waitForEvent(EVENT_SHOW, "reflow2");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("reflow2").hidden = false;
    });
    await waitForContentPaint(browser);
    await shown;
    for (const id of ["reflowContainer", "reflow2", "reflow3", "noReflow"]) {
      await testBoundsWithContent(docAcc, id, browser);
    }
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);
