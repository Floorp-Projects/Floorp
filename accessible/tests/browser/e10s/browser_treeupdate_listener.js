/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

addAccessibleTask('<span id="parent"><span id="child"></span></span>',
  async function(browser, accDoc) {
    is(findAccessibleChildByID(accDoc, "parent"), null,
      "Check that parent is not accessible.");
    is(findAccessibleChildByID(accDoc, "child"), null,
      "Check that child is not accessible.");

    let onReorder = waitForEvent(EVENT_REORDER, "body");
    // Add an event listener to parent.
    await ContentTask.spawn(browser, {}, () => {
      content.window.dummyListener = () => {};
      content.document.getElementById("parent").addEventListener(
        "click", content.window.dummyListener);
    });
    await onReorder;

    let tree = { TEXT: [] };
    testAccessibleTree(findAccessibleChildByID(accDoc, "parent"), tree);
  });
