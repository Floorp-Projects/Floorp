/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;

add_task(async function() {
  CustomizableUI.addWidgetToArea(
    "add-ons-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  info("Check addons button existence and functionality");

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let addonsButton = document.getElementById("add-ons-button");
  ok(addonsButton, "Add-ons button exists in Panel Menu");
  addonsButton.click();

  await Promise.all([
    TestUtils.waitForCondition(
      () => gBrowser.currentURI && gBrowser.currentURI.spec == "about:addons"
    ),
    new Promise(r =>
      gBrowser.selectedBrowser.addEventListener("load", r, true)
    ),
  ]);

  let addonsPage = gBrowser.selectedBrowser.contentWindow.document.getElementById(
    "addons-page"
  );
  ok(addonsPage, "Add-ons page was opened");
});

add_task(async function asyncCleanup() {
  CustomizableUI.reset();
  BrowserTestUtils.addTab(gBrowser, initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
  info("Tabs were restored");
});
