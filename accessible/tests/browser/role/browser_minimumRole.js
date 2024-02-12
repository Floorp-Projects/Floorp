/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Test that popover gets a minimum role.
 */
addAccessibleTask(
  `
<div id="generic" popover>generic</div>
<div id="alert" role="alert" popover>alert</div>
<blockquote id="blockquote" popover>blockquote</div>
  `,
  async function testPopover(browser, docAcc) {
    let generic = findAccessibleChildByID(docAcc, "generic");
    ok(!generic, "generic doesn't have an Accessible");
    info("Showing generic");
    let shown = waitForEvent(EVENT_SHOW, "generic");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("generic").showPopover();
    });
    generic = (await shown).accessible;
    testRole(generic, ROLE_GROUPING, "generic has minimum role group");
    info("Setting popover to null on generic");
    // Setting popover to null causes the Accessible to be recreated.
    shown = waitForEvent(EVENT_SHOW, "generic");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("generic").popover = null;
    });
    generic = (await shown).accessible;
    testRole(generic, ROLE_SECTION, "generic has generic role");

    let alert = findAccessibleChildByID(docAcc, "alert");
    ok(!alert, "alert doesn't have an Accessible");
    info("Showing alert");
    shown = waitForEvent(EVENT_SHOW, "alert");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("alert").showPopover();
    });
    alert = (await shown).accessible;
    testRole(alert, ROLE_ALERT, "alert has role alert");

    let blockquote = findAccessibleChildByID(docAcc, "blockquote");
    ok(!blockquote, "blockquote doesn't have an Accessible");
    info("Showing blockquote");
    shown = waitForEvent(EVENT_SHOW, "blockquote");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("blockquote").showPopover();
    });
    blockquote = (await shown).accessible;
    testRole(blockquote, ROLE_BLOCKQUOTE, "blockquote has role blockquote");
  },
  { chrome: true, topLevel: true }
);
