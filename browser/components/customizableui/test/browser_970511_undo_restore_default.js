/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Restoring default should show an "undo" option which undoes the restoring operation.
add_task(function() {
  let homeButtonId = "home-button";
  CustomizableUI.removeWidgetFromArea(homeButtonId);
  yield startCustomizing();
  ok(!CustomizableUI.inDefaultState, "Not in default state to begin with");
  is(CustomizableUI.getPlacementOfWidget(homeButtonId), null, "Home button is in palette");
  let undoReset = document.getElementById("customization-undo-reset");
  is(undoReset.hidden, true, "The undo button is hidden before reset");

  yield gCustomizeMode.reset();

  ok(CustomizableUI.inDefaultState, "In default state after reset");
  is(undoReset.hidden, false, "The undo button is visible after reset");

  undoReset.click();
  yield waitForCondition(function() !gCustomizeMode.resetting);
  ok(!CustomizableUI.inDefaultState, "Not in default state after reset-undo");
  is(undoReset.hidden, true, "The undo button is hidden after clicking on the undo button");
  is(CustomizableUI.getPlacementOfWidget(homeButtonId), null, "Home button is in palette");

  yield gCustomizeMode.reset();
});

// Performing an action after a reset will hide the reset button.
add_task(function() {
  let homeButtonId = "home-button";
  CustomizableUI.removeWidgetFromArea(homeButtonId);
  ok(!CustomizableUI.inDefaultState, "Not in default state to begin with");
  is(CustomizableUI.getPlacementOfWidget(homeButtonId), null, "Home button is in palette");
  let undoReset = document.getElementById("customization-undo-reset");
  is(undoReset.hidden, true, "The undo button is hidden before reset");

  yield gCustomizeMode.reset();

  ok(CustomizableUI.inDefaultState, "In default state after reset");
  is(undoReset.hidden, false, "The undo button is visible after reset");

  CustomizableUI.addWidgetToArea(homeButtonId, CustomizableUI.AREA_PANEL);
  is(undoReset.hidden, true, "The undo button is hidden after another change");
});

// "Restore defaults", exiting customize, and re-entering shouldn't show the Undo button
add_task(function() {
  let undoReset = document.getElementById("customization-undo-reset");
  is(undoReset.hidden, true, "The undo button is hidden before a reset");
  ok(!CustomizableUI.inDefaultState, "The browser should not be in default state");
  yield gCustomizeMode.reset();

  is(undoReset.hidden, false, "The undo button is hidden after a reset");
  yield endCustomizing();
  yield startCustomizing();
  is(undoReset.hidden, true, "The undo reset button should be hidden after entering customization mode");
});

add_task(function asyncCleanup() {
  yield gCustomizeMode.reset();
  yield endCustomizing();
});
