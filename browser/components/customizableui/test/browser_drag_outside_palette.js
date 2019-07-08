/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that moving items from the toolbar or panel to the palette by
 * dropping on the panel container (not inside the visible panel) works.
 */
add_task(async function() {
  await startCustomizing();
  let panelContainer = document.getElementById("customization-panel-container");
  // Try dragging an item from the navbar:
  let homeButton = document.getElementById("home-button");
  let oldNavbarPlacements = CustomizableUI.getWidgetIdsInArea("nav-bar");
  simulateItemDrag(homeButton, panelContainer);
  assertAreaPlacements(
    CustomizableUI.AREA_NAVBAR,
    oldNavbarPlacements.filter(w => w != "home-button")
  );
  ok(
    homeButton.closest("#customization-palette"),
    "Button should be in the palette"
  );

  // Put it in the panel and try again from there:
  let panelHolder = document.getElementById("customization-panelHolder");
  simulateItemDrag(homeButton, panelHolder);
  assertAreaPlacements(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, [
    "home-button",
  ]);

  simulateItemDrag(homeButton, panelContainer);
  assertAreaPlacements(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, []);

  ok(
    homeButton.closest("#customization-palette"),
    "Button should be in the palette"
  );

  // Check we can't move non-removable items like this:
  let urlbar = document.getElementById("urlbar-container");
  simulateItemDrag(urlbar, panelContainer);
  assertAreaPlacements(
    CustomizableUI.AREA_NAVBAR,
    oldNavbarPlacements.filter(w => w != "home-button")
  );
});

registerCleanupFunction(async function() {
  await gCustomizeMode.reset();
  await endCustomizing();
});
