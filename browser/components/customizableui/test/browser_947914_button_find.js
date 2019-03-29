/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  info("Check find button existence and functionality");
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  CustomizableUI.addWidgetToArea("find-button", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  registerCleanupFunction(() => CustomizableUI.reset());

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let findButton = document.getElementById("find-button");
  ok(findButton, "Find button exists in Panel Menu");

  let findBarPromise = gBrowser.isFindBarInitialized() ?
    null : BrowserTestUtils.waitForEvent(gBrowser.selectedTab, "TabFindInitialized");

  findButton.click();
  await findBarPromise;
  ok(!gFindBar.hasAttribute("hidden"), "Findbar opened successfully");

  // close find bar
  gFindBar.close();
  info("Findbar was closed");
});
