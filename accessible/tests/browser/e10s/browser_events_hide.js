/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test hide event and its interface:
 *   - targetParent
 *   - targetNextSibling
 *   - targetPrevSibling
 */
addAccessibleTask(
  `
  <div id="parent">
    <div id="previous"></div>
    <div id="to-hide"></div>
    <div id="next"></div>
  </div>`,
  async function (browser, accDoc) {
    let acc = findAccessibleChildByID(accDoc, "to-hide");
    let onHide = waitForEvent(EVENT_HIDE, acc);
    await invokeSetStyle(browser, "to-hide", "visibility", "hidden");
    let event = await onHide;
    let hideEvent = event.QueryInterface(Ci.nsIAccessibleHideEvent);

    is(
      getAccessibleDOMNodeID(hideEvent.targetParent),
      "parent",
      "Correct target parent."
    );
    is(
      getAccessibleDOMNodeID(hideEvent.targetNextSibling),
      "next",
      "Correct target next sibling."
    );
    is(
      getAccessibleDOMNodeID(hideEvent.targetPrevSibling),
      "previous",
      "Correct target previous sibling."
    );
  },
  { iframe: true, remoteIframe: true }
);
