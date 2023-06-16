/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask(
  '<ol id="list"></ol>',
  async function (browser, accDoc) {
    let list = findAccessibleChildByID(accDoc, "list");

    testAccessibleTree(list, {
      role: ROLE_LIST,
      children: [],
    });

    await invokeSetAttribute(
      browser,
      currentContentDoc(),
      "contentEditable",
      "true"
    );
    let onReorder = waitForEvent(EVENT_REORDER, "list");
    await invokeContentTask(browser, [], () => {
      let li = content.document.createElement("li");
      li.textContent = "item";
      content.document.getElementById("list").appendChild(li);
    });
    await onReorder;

    testAccessibleTree(list, {
      role: ROLE_LIST,
      children: [
        {
          role: ROLE_LISTITEM,
          children: [
            { role: ROLE_LISTITEM_MARKER, name: "1. ", children: [] },
            { role: ROLE_TEXT_LEAF, children: [] },
          ],
        },
      ],
    });
  },
  { iframe: true, remoteIframe: true }
);
