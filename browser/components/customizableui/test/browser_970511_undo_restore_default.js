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
  let undoResetButton = document.getElementById("customization-undo-reset-button");
  is(undoResetButton.hidden, true, "The undo button is hidden before reset");

  yield gCustomizeMode.reset();

  ok(CustomizableUI.inDefaultState, "In default state after reset");
  is(undoResetButton.hidden, false, "The undo button is visible after reset");

  undoResetButton.click();
  yield waitForCondition(function() !gCustomizeMode.resetting);
  ok(!CustomizableUI.inDefaultState, "Not in default state after reset-undo");
  is(undoResetButton.hidden, true, "The undo button is hidden after clicking on the undo button");
  is(CustomizableUI.getPlacementOfWidget(homeButtonId), null, "Home button is in palette");

  yield gCustomizeMode.reset();
});

// Performing an action after a reset will hide the reset button.
add_task(function() {
  let homeButtonId = "home-button";
  CustomizableUI.removeWidgetFromArea(homeButtonId);
  ok(!CustomizableUI.inDefaultState, "Not in default state to begin with");
  is(CustomizableUI.getPlacementOfWidget(homeButtonId), null, "Home button is in palette");
  let undoResetButton = document.getElementById("customization-undo-reset-button");
  is(undoResetButton.hidden, true, "The undo button is hidden before reset");

  yield gCustomizeMode.reset();

  ok(CustomizableUI.inDefaultState, "In default state after reset");
  is(undoResetButton.hidden, false, "The undo button is visible after reset");

  CustomizableUI.addWidgetToArea(homeButtonId, CustomizableUI.AREA_PANEL);
  is(undoResetButton.hidden, true, "The undo button is hidden after another change");
});

// "Restore defaults", exiting customize, and re-entering shouldn't show the Undo button
add_task(function() {
  let undoResetButton = document.getElementById("customization-undo-reset-button");
  is(undoResetButton.hidden, true, "The undo button is hidden before a reset");
  ok(!CustomizableUI.inDefaultState, "The browser should not be in default state");
  yield gCustomizeMode.reset();

  is(undoResetButton.hidden, false, "The undo button is visible after a reset");
  yield endCustomizing();
  yield startCustomizing();
  is(undoResetButton.hidden, true, "The undo reset button should be hidden after entering customization mode");
});

// Bug 971626 - Restore Defaults should collapse the Title Bar
add_task(function() {
  if (Services.appinfo.OS != "WINNT" &&
      Services.appinfo.OS != "Darwin") {
    return;
  }
  let prefName = "browser.tabs.drawInTitlebar";
  let defaultValue = Services.prefs.getBoolPref(prefName);
  let restoreDefaultsButton = document.getElementById("customization-reset-button");
  let titleBarButton = document.getElementById("customization-titlebar-visibility-button");
  let undoResetButton = document.getElementById("customization-undo-reset-button");
  ok(CustomizableUI.inDefaultState, "Should be in default state at start of test");
  ok(restoreDefaultsButton.disabled, "Restore defaults button should be disabled when in default state");
  is(titleBarButton.hasAttribute("checked"), !defaultValue, "Title bar button should reflect pref value");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden at start of test");

  Services.prefs.setBoolPref(prefName, !defaultValue);
  ok(!restoreDefaultsButton.disabled, "Restore defaults button should be enabled when pref changed");
  is(titleBarButton.hasAttribute("checked"), defaultValue, "Title bar button should reflect changed pref value");
  ok(!CustomizableUI.inDefaultState, "With titlebar flipped, no longer default");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden after pref change");

  yield gCustomizeMode.reset();
  ok(restoreDefaultsButton.disabled, "Restore defaults button should be disabled after reset");
  is(titleBarButton.hasAttribute("checked"), !defaultValue, "Title bar button should reflect default value after reset");
  is(Services.prefs.getBoolPref(prefName), defaultValue, "Reset should reset drawInTitlebar");
  ok(CustomizableUI.inDefaultState, "In default state after titlebar reset");
  is(undoResetButton.hidden, false, "Undo reset button should be visible after reset");
  ok(!undoResetButton.disabled, "Undo reset button should be enabled after reset");

  yield gCustomizeMode.undoReset();
  ok(!restoreDefaultsButton.disabled, "Restore defaults button should be enabled after undo-reset");
  is(titleBarButton.hasAttribute("checked"), defaultValue, "Title bar button should reflect undo-reset value");
  ok(!CustomizableUI.inDefaultState, "No longer in default state after undo");
  is(Services.prefs.getBoolPref(prefName), !defaultValue, "Undo-reset goes back to previous pref value");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden after undo-reset clicked");

  Services.prefs.clearUserPref(prefName);
  ok(CustomizableUI.inDefaultState, "In default state after pref cleared");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden at end of test");
});

// Bug 1082108 - Restore Defaults should clear user pref for devedition theme
add_task(function() {
  let prefName = "browser.devedition.theme.enabled";
  Services.prefs.setBoolPref("browser.devedition.theme.showCustomizeButton", true);
  let defaultValue = Services.prefs.getBoolPref(prefName);
  let restoreDefaultsButton = document.getElementById("customization-reset-button");
  let deveditionThemeButton = document.getElementById("customization-devedition-theme-button");
  let undoResetButton = document.getElementById("customization-undo-reset-button");
  ok(CustomizableUI.inDefaultState, "Should be in default state at start of test");
  ok(restoreDefaultsButton.disabled, "Restore defaults button should be disabled when in default state");
  is(deveditionThemeButton.hasAttribute("checked"), defaultValue, "Devedition theme button should reflect pref value");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden at start of test");
  Services.prefs.setBoolPref(prefName, !defaultValue);
  ok(!restoreDefaultsButton.disabled, "Restore defaults button should be enabled when pref changed");
  is(deveditionThemeButton.hasAttribute("checked"), !defaultValue, "Devedition theme button should reflect changed pref value");
  ok(!CustomizableUI.inDefaultState, "With devedition theme flipped, no longer default");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden after pref change");

  yield gCustomizeMode.reset();
  ok(restoreDefaultsButton.disabled, "Restore defaults button should be disabled after reset");
  is(deveditionThemeButton.hasAttribute("checked"), defaultValue, "devedition theme button should reflect default value after reset");
  is(Services.prefs.getBoolPref(prefName), defaultValue, "Reset should reset devedition.theme.enabled");
  ok(CustomizableUI.inDefaultState, "In default state after devedition theme reset");
  is(undoResetButton.hidden, false, "Undo reset button should be visible after reset");
  ok(!undoResetButton.disabled, "Undo reset button should be enabled after reset");

  yield gCustomizeMode.undoReset();
  ok(!restoreDefaultsButton.disabled, "Restore defaults button should be enabled after undo-reset");
  is(deveditionThemeButton.hasAttribute("checked"), !defaultValue, "devedition theme button should reflect undo-reset value");
  ok(!CustomizableUI.inDefaultState, "No longer in default state after undo");
  is(Services.prefs.getBoolPref(prefName), !defaultValue, "Undo-reset goes back to previous pref value");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden after undo-reset clicked");

  Services.prefs.clearUserPref(prefName);
  ok(CustomizableUI.inDefaultState, "In default state after pref cleared");
  is(undoResetButton.hidden, true, "Undo reset button should be hidden at end of test");
});

add_task(function asyncCleanup() {
  yield gCustomizeMode.reset();
  yield endCustomizing();
});
