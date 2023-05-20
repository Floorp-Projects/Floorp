/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test UniqueID property.
 */
addAccessibleTask(
  '<div id="div"></div>',
  async function (browser, accDoc) {
    const div = findAccessibleChildByID(accDoc, "div");
    const accUniqueID = await invokeContentTask(browser, [], () => {
      const accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );

      return accService.getAccessibleFor(content.document.getElementById("div"))
        .uniqueID;
    });

    is(
      accUniqueID,
      div.uniqueID,
      "Both proxy and the accessible return correct unique ID."
    );
  },
  { iframe: true, remoteIframe: true }
);
