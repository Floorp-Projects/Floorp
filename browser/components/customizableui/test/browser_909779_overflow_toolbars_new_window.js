/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Resize to a small window, open a new window, check that new window handles overflow properly
add_task(async function () {
  let originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );
  let oldChildCount =
    CustomizableUI.getCustomizationTarget(navbar).childElementCount;
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  Assert.less(
    CustomizableUI.getCustomizationTarget(navbar).childElementCount,
    oldChildCount,
    "Should have fewer children."
  );
  let newWindow = await openAndLoadWindow();
  let otherNavBar = newWindow.document.getElementById(
    CustomizableUI.AREA_NAVBAR
  );
  await TestUtils.waitForCondition(() =>
    otherNavBar.hasAttribute("overflowing")
  );
  ok(
    otherNavBar.hasAttribute("overflowing"),
    "Other window should have an overflowing toolbar."
  );
  await promiseWindowClosed(newWindow);

  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should no longer have an overflowing toolbar."
  );
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
