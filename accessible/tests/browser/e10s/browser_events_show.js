/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test show event
 */
addAccessibleTask(
  '<div id="div" style="visibility: hidden;"></div>',
  async function (browser) {
    let onShow = waitForEvent(EVENT_SHOW, "div");
    await invokeSetStyle(browser, "div", "visibility", "visible");
    let showEvent = await onShow;
    ok(
      showEvent.accessibleDocument instanceof nsIAccessibleDocument,
      "Accessible document not present."
    );
  },
  { iframe: true, remoteIframe: true }
);
