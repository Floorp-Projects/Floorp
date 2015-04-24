/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

// One orphaned item should have two placeholders next to it.
add_task(function() {
  yield startCustomizing();

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_PANEL);
    ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  }
  if (!isInDevEdition()) {
    ok(CustomizableUI.inDefaultState, "Should be in default state.");
  } else {
    ok(!CustomizableUI.inDefaultState, "Should not be in default state if on DevEdition.");
  }

  let btn = document.getElementById("open-file-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);
  is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders after re-entering");

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR, 2);
  }

  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// Two orphaned items should have one placeholder next to them (case 1).
add_task(function() {
  yield startCustomizing();

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_PANEL);
  }

  let btn = document.getElementById("open-file-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);
  let placementsAfterAppend = placements;

  placementsAfterAppend = placements.concat(["open-file-button"]);
  simulateItemDrag(btn, panel);

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);

  ok(!CustomizableUI.inDefaultState, "Should not be in default state.");

  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder after re-entering");

  let palette = document.getElementById("customization-palette");
  simulateItemDrag(btn, palette);

  btn = document.getElementById("open-file-button");
  simulateItemDrag(btn, palette);

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR, 2);
  }

  ok(CustomizableUI.inDefaultState, "Should be in default state again."); 
});

// Two orphaned items should have one placeholder next to them (case 2).
add_task(function() {
  yield startCustomizing();

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_PANEL);
  }

  let btn = document.getElementById("add-ons-button");
  let btn2 = document.getElementById("developer-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let palette = document.getElementById("customization-palette");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  let placementsAfterAppend = placements.filter(p => p != btn.id && p != btn2.id);
  simulateItemDrag(btn, palette);
  simulateItemDrag(btn2, palette);

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholder after re-entering");

  simulateItemDrag(btn, panel);
  simulateItemDrag(btn2, panel);

  assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR, 2);
  }

  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// A wide widget at the bottom of the panel should have three placeholders after it.
add_task(function() {
  yield startCustomizing();

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_PANEL);
  }

  let btn = document.getElementById("edit-controls");
  let btn2 = document.getElementById("developer-button");
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  let palette = document.getElementById("customization-palette");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

  placements.pop();
  simulateItemDrag(btn2, palette);

  let placementsAfterAppend = placements.concat([placements.shift()]);
  simulateItemDrag(btn, panel);
  assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders before exiting");

  yield endCustomizing();
  yield startCustomizing();
  is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders after re-entering");

  simulateItemDrag(btn2, panel);

  let zoomControls = document.getElementById("zoom-controls");
  simulateItemDrag(btn, zoomControls);

  if (isInDevEdition()) {
    CustomizableUI.addWidgetToArea("developer-button", CustomizableUI.AREA_NAVBAR, 2);
  }

  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// The default placements should have two placeholders at the bottom (or 1 in win8).
add_task(function() {
  yield startCustomizing();
  let numPlaceholders = -1;

  if (isInDevEdition()) {
    numPlaceholders = 3;
  } else {
    numPlaceholders = 2;
  }

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
