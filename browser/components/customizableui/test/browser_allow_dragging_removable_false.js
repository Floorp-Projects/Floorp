"use strict";

/**
 * Test dragging a removable=false widget within its own area as well as to the palette.
 */
add_task(async function() {
  await startCustomizing();
  let forwardButton = document.getElementById("forward-button");
  is(forwardButton.getAttribute("removable"), "false", "forward-button should not be removable");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  let urlbarContainer = document.getElementById("urlbar-container");
  let placementsAfterDrag = getAreaWidgetIds(CustomizableUI.AREA_NAVBAR);
  placementsAfterDrag.splice(placementsAfterDrag.indexOf("forward-button"), 1);
  placementsAfterDrag.splice(placementsAfterDrag.indexOf("urlbar-container"), 0, "forward-button");

  // Force layout flush to ensure the drag completes as expected
  urlbarContainer.clientWidth;

  simulateItemDrag(forwardButton, urlbarContainer, "start");
  assertAreaPlacements(CustomizableUI.AREA_NAVBAR, placementsAfterDrag);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(forwardButton, palette);
  is(CustomizableUI.getPlacementOfWidget("forward-button").area, CustomizableUI.AREA_NAVBAR, "forward-button was not able to move to palette");

  await endCustomizing();
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});
