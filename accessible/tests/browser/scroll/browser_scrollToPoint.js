/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

/**
 * Test nsIAccessible::scrollToPoint.
 */
addAccessibleTask(
  `
<hr style="height: 100vh;">
<p id="p">hi</p>
<hr style="height: 100vh;">
  `,
  async function (browser, docAcc) {
    const [docX, docY] = getPos(docAcc);
    const p = findAccessibleChildByID(docAcc, "p");
    const [pX] = getPos(p);
    info("Scrolling p");
    let scrolled = waitForEvent(EVENT_SCROLLING_END, docAcc);
    p.scrollToPoint(COORDTYPE_SCREEN_RELATIVE, docX, docY);
    await scrolled;
    // We can only scroll this vertically.
    testPos(p, [pX, docY]);
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);
