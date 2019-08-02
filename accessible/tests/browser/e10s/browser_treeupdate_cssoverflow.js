/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  `
  <div id="container"><div id="scrollarea" style="overflow:auto;"><input>`,
  async function(browser, accDoc) {
    const id1 = "container";
    const container = findAccessibleChildByID(accDoc, id1);

    /* ================= Change scroll range ================================== */
    let tree = {
      SECTION: [
        {
          // container
          SECTION: [
            {
              // scroll area
              ENTRY: [], // child content
            },
          ],
        },
      ],
    };
    testAccessibleTree(container, tree);

    let onReorder = waitForEvent(EVENT_REORDER, id1);
    await ContentTask.spawn(browser, id1, id => {
      let doc = content.document;
      doc.getElementById("scrollarea").style.width = "20px";
      doc.getElementById(id).appendChild(doc.createElement("input"));
    });
    await onReorder;

    tree = {
      SECTION: [
        {
          // container
          SECTION: [
            {
              // scroll area
              ENTRY: [], // child content
            },
          ],
        },
        {
          ENTRY: [], // inserted input
        },
      ],
    };
    testAccessibleTree(container, tree);
  }
);
