/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that after a reset, we can still drag special nodes in customize mode
 */
add_task(async function() {
  await startCustomizing();
  CustomizableUI.addWidgetToArea("spring", "nav-bar", 5);
  await gCustomizeMode.reset();
  let springs = document.querySelectorAll("#nav-bar toolbarspring");
  let lastSpring = springs[springs.length - 1];
  let expectedPlacements = CustomizableUI.getWidgetIdsInArea("nav-bar");
  info("Placements before drag: " + expectedPlacements.join(","));
  let lastItem = document.getElementById(expectedPlacements[expectedPlacements.length - 1]);
  await waitForElementShown(lastItem);
  simulateItemDrag(lastSpring, lastItem, "end");
  expectedPlacements.splice(expectedPlacements.indexOf(lastSpring.id), 1);
  expectedPlacements.push(lastSpring.id);
  let actualPlacements = CustomizableUI.getWidgetIdsInArea("nav-bar");
  // Log these separately because Assert.deepEqual truncates the stringified versions...
  info("Actual placements: " + actualPlacements.join(","));
  info("Expected placements: " + expectedPlacements.join(","));
  Assert.deepEqual(expectedPlacements, actualPlacements, "Should be able to move spring");
  await gCustomizeMode.reset();
  await endCustomizing();
});
