/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await startCustomizing();
  let themesButton = document.querySelector("#customization-lwtheme-button");
  ok(!themesButton.hidden, "Customize Theme button is visible.");
  let aboutAddonsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:addons"
  );
  themesButton.click();
  await aboutAddonsPromise;
  // Removing about:addons tab.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await endCustomizing();
  // Removing tab that held the customize window.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
