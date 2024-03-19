/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

var newTab = null;

add_task(async function () {
  info("Check preferences button existence and functionality");
  CustomizableUI.addWidgetToArea(
    "preferences-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  registerCleanupFunction(() => CustomizableUI.reset());

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let preferencesButton = document.getElementById("preferences-button");
  ok(preferencesButton, "Preferences button exists in Panel Menu");
  preferencesButton.click();

  newTab = gBrowser.selectedTab;
  await waitForPageLoad(newTab);

  let openedPage = gBrowser.currentURI.spec;
  is(openedPage, "about:preferences", "Preferences page was opened");
});

add_task(function asyncCleanup() {
  if (gBrowser.tabs.length == 1) {
    BrowserTestUtils.addTab(gBrowser, "about:blank");
  }

  gBrowser.removeTab(gBrowser.selectedTab);
  info("Tabs were restored");
});

function waitForPageLoad(aTab) {
  return new Promise((resolve, reject) => {
    let timeoutId = setTimeout(() => {
      aTab.linkedBrowser.removeEventListener("load", onTabLoad, true);
      reject("Page didn't load within " + 20000 + "ms");
    }, 20000);

    async function onTabLoad() {
      clearTimeout(timeoutId);
      aTab.linkedBrowser.removeEventListener("load", onTabLoad, true);
      info("Tab event received: load");
      resolve();
    }

    aTab.linkedBrowser.addEventListener("load", onTabLoad, true, true);
  });
}
