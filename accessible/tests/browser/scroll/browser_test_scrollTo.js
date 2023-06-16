/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test nsIAccessible::scrollTo.
 */
addAccessibleTask(
  `
<div id="scroller" style="height: 1px; overflow: scroll;">
  <p id="p1">a</p>
  <p id="p2">b</p>
</div>
  `,
  async function (browser, docAcc) {
    const scroller = findAccessibleChildByID(docAcc, "scroller");
    // scroller can only fit one of p1 or p2, not both.
    // p1 is on screen already.
    const p2 = findAccessibleChildByID(docAcc, "p2");
    info("scrollTo p2");
    let scrolled = waitForEvent(
      nsIAccessibleEvent.EVENT_SCROLLING_END,
      scroller
    );
    p2.scrollTo(SCROLL_TYPE_ANYWHERE);
    await scrolled;
    const p1 = findAccessibleChildByID(docAcc, "p1");
    info("scrollTo p1");
    scrolled = waitForEvent(nsIAccessibleEvent.EVENT_SCROLLING_END, scroller);
    p1.scrollTo(SCROLL_TYPE_ANYWHERE);
    await scrolled;
  },
  { topLevel: true, iframe: true, remoteIframe: true, chrome: true }
);
