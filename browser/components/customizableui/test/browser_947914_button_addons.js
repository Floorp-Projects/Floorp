/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;

add_task(async function() {
  if (
    Services.prefs.getBoolPref("extensions.unifiedExtensions.enabled", false)
  ) {
    // When this pref is enabled, the "Add-ons and themes" button no longer
    // exists so we cannot test it... In the future, we'll want to remove this
    // test entirely since it won't be relevant when the unified extensions UI
    // will be enabled on all channels.
    ok(true, "Skip task because unifiedExtensions pref is enabled");
    return;
  }

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

  let addonsPage = gBrowser.selectedBrowser.contentWindow.document.querySelector(
    "title[data-l10n-id='addons-page-title']"
  );
  ok(addonsPage, "Add-ons page was opened");
});

add_task(async function asyncCleanup() {
  if (
    Services.prefs.getBoolPref("extensions.unifiedExtensions.enabled", false)
  ) {
    // The comment in the task above also applies here.
    ok(true, "Skip task because unifiedExtensions pref is enabled");
    return;
  }

  CustomizableUI.reset();
  BrowserTestUtils.addTab(gBrowser, initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
  info("Tabs were restored");
});
