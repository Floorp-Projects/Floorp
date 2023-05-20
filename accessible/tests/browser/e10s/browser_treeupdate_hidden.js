/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function setHidden(browser, value) {
  let onReorder = waitForEvent(EVENT_REORDER, "container");
  await invokeSetAttribute(browser, "child", "hidden", value);
  await onReorder;
}

addAccessibleTask(
  '<div id="container"><input id="child"></div>',
  async function (browser, accDoc) {
    let container = findAccessibleChildByID(accDoc, "container");

    testAccessibleTree(container, { SECTION: [{ ENTRY: [] }] });

    // Set @hidden attribute
    await setHidden(browser, "true");
    testAccessibleTree(container, { SECTION: [] });

    // Remove @hidden attribute
    await setHidden(browser);
    testAccessibleTree(container, { SECTION: [{ ENTRY: [] }] });
  },
  { iframe: true, remoteIframe: true }
);
