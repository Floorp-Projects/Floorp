/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

registerCleanupFunction(async function asyncCleanup() {
  await resetCustomization();
});

// Resize to a small window, resize back, shouldn't affect default state.
add_task(async function () {
  let originalWindowWidth = window.outerWidth;
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  let navbarTarget = CustomizableUI.getCustomizationTarget(navbar);
  let oldChildCount = navbarTarget.childElementCount;
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(
    () => navbar.hasAttribute("overflowing"),
    "Navbar has a overflowing attribute"
  );
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(
    CustomizableUI.inDefaultState,
    "Should still be in default state when overflowing."
  );
  Assert.less(
    navbarTarget.childElementCount,
    oldChildCount,
    "Should have fewer children."
  );
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(
    () => !navbar.hasAttribute("overflowing"),
    "Navbar does not have an overflowing attribute"
  );
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should no longer have an overflowing toolbar."
  );
  ok(
    CustomizableUI.inDefaultState,
    "Should still be in default state now we're no longer overflowing."
  );

  // Verify actual physical placements match those of the placement array:
  let placementCounter = 0;
  let placements = CustomizableUI.getWidgetIdsInArea(
    CustomizableUI.AREA_NAVBAR
  );
  for (let node of navbarTarget.children) {
    if (node.getAttribute("skipintoolbarset") == "true") {
      continue;
    }
    is(
      placements[placementCounter++],
      node.id,
      "Nodes should match after overflow"
    );
  }
  is(
    placements.length,
    placementCounter,
    "Should have as many nodes as expected"
  );
  is(
    navbarTarget.childElementCount,
    oldChildCount,
    "Number of nodes should match"
  );
});

// Enter and exit customization mode, check that default state is correct.
add_task(async function () {
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  await startCustomizing();
  ok(
    CustomizableUI.inDefaultState,
    "Should be in default state in customization mode."
  );
  await endCustomizing();
  ok(
    CustomizableUI.inDefaultState,
    "Should be in default state after customization mode."
  );
});
