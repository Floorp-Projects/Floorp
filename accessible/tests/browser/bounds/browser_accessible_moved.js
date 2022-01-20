/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function assertBoundsNonZero(acc) {
  // XXX We don't use getBounds because it uses BoundsInCSSPixels(), but that
  // isn't implemented for the cache yet.
  let x = {};
  let y = {};
  let width = {};
  let height = {};
  acc.getBounds(x, y, width, height);
  ok(x.value > 0, "x is non-0");
  ok(y.value > 0, "y is non-0");
  ok(width.value > 0, "width is non-0");
  ok(height.value > 0, "height is non-0");
}

/**
 * Test that bounds aren't 0 after an Accessible is moved (but not re-created).
 */
addAccessibleTask(
  `
<div id="root" role="group"><div id="scrollable" role="presentation" style="height: 1px;"><button id="button">test</button></div></div>
  `,
  async function(browser, docAcc) {
    let button = findAccessibleChildByID(docAcc, "button");
    assertBoundsNonZero(button);

    const root = findAccessibleChildByID(docAcc, "root");
    let reordered = waitForEvent(EVENT_REORDER, root);
    // scrollable wasn't in the a11y tree, but this will force it to be created.
    // button will be moved inside it.
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("scrollable").style.overflow = "scroll";
    });
    await reordered;

    const scrollable = findAccessibleChildByID(docAcc, "scrollable");
    assertBoundsNonZero(scrollable);
    // XXX button's RemoteAccessible was recreated, so we have to fetch it
    // again. This shouldn't be necessary once bug 1739050 is fixed.
    button = findAccessibleChildByID(docAcc, "button");
    assertBoundsNonZero(button);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);
