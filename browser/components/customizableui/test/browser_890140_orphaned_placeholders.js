/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

// One orphaned item should have two placeholders next to it.
add_task(function() {
  yield startCustomizing();
  let btn = document.getElementById("open-file-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);

  CustomizableUI.removeWidgetFromArea("social-share-button");
  if (isInWin8()) {
    CustomizableUI.removeWidgetFromArea("switch-to-metro-button");
  }
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);
  is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders after re-entering");

  CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_PANEL);
  if (isInWin8()) {
    CustomizableUI.addWidgetToArea("switch-to-metro-button", CustomizableUI.AREA_PANEL);
  }
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// Two orphaned items should have one placeholder next to them (case 1).
add_task(function() {
  yield startCustomizing();
  let btn = document.getElementById("open-file-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  CustomizableUI.removeWidgetFromArea("social-share-button");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  let placementsAfterAppend = placements;

  if (!isInWin8()) {
    placementsAfterAppend = placements.concat(["open-file-button"]);
    simulateItemDrag(btn, panel);
  }

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should not be in default state");
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder after re-entering");

  let palette = document.getElementById("customization-palette");
  simulateItemDrag(btn, palette);

  if (!isInWin8()) {
    btn = document.getElementById("open-file-button");
    simulateItemDrag(btn, palette);
  }
  CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_PANEL);
  ok(CustomizableUI.inDefaultState, "Should be in default state again."); 
});

// Two orphaned items should have one placeholder next to them (case 2).
add_task(function() {
  yield startCustomizing();
  let buttonsToMove = ["add-ons-button", "developer-button", "social-share-button"];
  if (isInWin8()) {
    buttonsToMove.push("switch-to-metro-button");
  }
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let palette = document.getElementById("customization-palette");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  let placementsAfterAppend = placements.filter(p => buttonsToMove.indexOf(p) < 0);
  for (let i in buttonsToMove) {
    CustomizableUI.removeWidgetFromArea(buttonsToMove[i]);
  }

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder after re-entering");

  for (let i in buttonsToMove) {
    CustomizableUI.addWidgetToArea(buttonsToMove[i], CustomizableUI.AREA_PANEL);
  }

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// A wide widget at the bottom of the panel should have three placeholders after it.
add_task(function() {
  yield startCustomizing();
  let btn = document.getElementById("edit-controls");
  let developerButton = document.getElementById("developer-button");
  let metroBtn = document.getElementById("switch-to-metro-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let palette = document.getElementById("customization-palette");
  CustomizableUI.removeWidgetFromArea("social-share-button");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  placements.pop();
  simulateItemDrag(developerButton, palette);
  if (isInWin8()) {
    // Remove switch-to-metro-button
    placements.pop();
    simulateItemDrag(metroBtn, palette);
  }

  let placementsAfterAppend = placements.concat([placements.shift()]);
  simulateItemDrag(btn, panel);
  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders after re-entering");

  simulateItemDrag(developerButton, panel);
  CustomizableUI.addWidgetToArea("social-share-button", CustomizableUI.AREA_PANEL);
  if (isInWin8()) {
    simulateItemDrag(metroBtn, panel);
  }
  let zoomControls = document.getElementById("zoom-controls");
  simulateItemDrag(btn, zoomControls);
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// The default placements should have one placeholder at the bottom (or 3 in metro-enabled win8).
add_task(function() {
  yield startCustomizing();
  let numPlaceholders = isInWin8() ? 3 : 1;
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  ok(CustomizableUI.inDefaultState, "Should be in default state.");
  is(getVisiblePlaceholderCount(panel), numPlaceholders, "Should have " + numPlaceholders + " visible placeholders before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), numPlaceholders, "Should have " + numPlaceholders + " visible placeholders after re-entering");

  ok(CustomizableUI.inDefaultState, "Should still be in default state.");
});

add_task(function asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});

function getVisiblePlaceholderCount(aPanel) {
  let visiblePlaceholders = aPanel.querySelectorAll(".panel-customization-placeholder:not([hidden=true])");
  return visiblePlaceholders.length;
}
