/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Test ARIA Dialog
addAccessibleTask(
  "e10s/doc_treeupdate_ariadialog.html",
  async function(browser, accDoc) {
    testAccessibleTree(accDoc, {
      role: ROLE_DOCUMENT,
      children: [],
    });

    // Make dialog visible and update its inner content.
    let onShow = waitForEvent(EVENT_SHOW, "dialog");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("dialog").style.display = "block";
    });
    await onShow;

    testAccessibleTree(accDoc, {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_DIALOG,
          children: [
            {
              role: ROLE_PUSHBUTTON,
              children: [{ role: ROLE_TEXT_LEAF }],
            },
            {
              role: ROLE_ENTRY,
            },
          ],
        },
      ],
    });
  },
  { iframe: true, remoteIframe: true }
);
