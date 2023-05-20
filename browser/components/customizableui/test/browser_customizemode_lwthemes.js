/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  await startCustomizing();
  // Find the footer buttons to test.
  let manageLink = document.querySelector("#customization-lwtheme-link");

  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");
  manageLink.click();
  let addonsTab = await waitForNewTab;

  is(gBrowser.currentURI.spec, "about:addons", "Manage opened about:addons");
  BrowserTestUtils.removeTab(addonsTab);

  // Wait for customize mode to be re-entered now that the customize tab is
  // active. This is needed for endCustomizing() to work properly.
  await TestUtils.waitForCondition(
    () => document.documentElement.getAttribute("customizing") == "true"
  );
  await endCustomizing();
});
