/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getCenterY(acc) {
  const y = {};
  const h = {};
  acc.getBounds({}, y, {}, h);
  return y.value + h.value / 2;
}

/**
 * Test nsIAccessible::scrollTo.
 */
addAccessibleTask(
  `
<div id="scroller" style="height: 100vh; overflow: scroll;">
  <hr style="height: 100vh;">
  <p id="p1" style="height: 10vh;">a</p>
  <hr style="height: 100vh;">
  <p id="p2" style="height: 10vh;">b</p>
  <hr style="height: 100vh;">
</div>
  `,
  async function (browser, docAcc) {
    const scroller = findAccessibleChildByID(docAcc, "scroller");
    const scrollerY = getCenterY(scroller);
    // scroller can only show one of p1 or p2, not both.
    const p1 = findAccessibleChildByID(docAcc, "p1");
    info("scrollTo p1");
    let scrolled = waitForEvent(
      nsIAccessibleEvent.EVENT_SCROLLING_END,
      scroller
    );
    p1.scrollTo(SCROLL_TYPE_ANYWHERE);
    await scrolled;
    isWithin(getCenterY(p1), scrollerY, 10, "p1 scrolled to center");
    const p2 = findAccessibleChildByID(docAcc, "p2");
    info("scrollTo p2");
    scrolled = waitForEvent(nsIAccessibleEvent.EVENT_SCROLLING_END, scroller);
    p2.scrollTo(SCROLL_TYPE_ANYWHERE);
    await scrolled;
    isWithin(getCenterY(p2), scrollerY, 10, "p2 scrolled to center");
    info("scrollTo p1");
    scrolled = waitForEvent(nsIAccessibleEvent.EVENT_SCROLLING_END, scroller);
    p1.scrollTo(SCROLL_TYPE_ANYWHERE);
    await scrolled;
    isWithin(getCenterY(p1), scrollerY, 10, "p1 scrolled to center");
  },
  { topLevel: true, iframe: true, remoteIframe: true, chrome: true }
);
