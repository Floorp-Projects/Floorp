/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getSpringCount(area) {
  return CustomizableUI.getWidgetIdsInArea(area).filter(id =>
    id.includes("spring")
  ).length;
}

/**
 * Check that no matter where we add a flexible space, we
 * never end up without a flexible space in the palette.
 */
add_task(async function test_flexible_space_addition() {
  await startCustomizing();
  let palette = document.getElementById("customization-palette");
  // Make the bookmarks toolbar visible:
  CustomizableUI.setToolbarVisibility(CustomizableUI.AREA_BOOKMARKS, true);
  let areas = [CustomizableUI.AREA_NAVBAR, CustomizableUI.AREA_BOOKMARKS];
  if (AppConstants.platform != "macosx") {
    areas.push(CustomizableUI.AREA_MENUBAR);
  }

  for (let area of areas) {
    let spacer = palette.querySelector("toolbarspring");
    let toolbar = document.getElementById(area);
    toolbar = CustomizableUI.getCustomizationTarget(toolbar);

    let springCount = getSpringCount(area);
    simulateItemDrag(spacer, toolbar);
    // Check we added the spring:
    is(
      springCount + 1,
      getSpringCount(area),
      "Should now have an extra spring"
    );

    // Check there's still one in the palette:
    let newSpacer = palette.querySelector("toolbarspring");
    ok(newSpacer, "Should have created a new spring");
  }
});
registerCleanupFunction(async function asyncCleanup() {
  await endCustomizing();
  await resetCustomization();
});
