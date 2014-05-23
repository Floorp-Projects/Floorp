/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let bookmarksToolbar = document.getElementById("PersonalToolbar");
let navbar = document.getElementById("nav-bar");
let tabsToolbar = document.getElementById("TabsToolbar");

// Customization reset should restore visibility to default-visible toolbars.
add_task(function() {
  is(navbar.collapsed, false, "Test should start with navbar visible");
  setToolbarVisibility(navbar, false);
  is(navbar.collapsed, true, "navbar should be hidden now");

  yield resetCustomization();

  is(navbar.collapsed, false, "Customization reset should restore visibility to the navbar");
});

// Customization reset should restore collapsed-state to default-collapsed toolbars.
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");

  is(bookmarksToolbar.collapsed, true, "Test should start with bookmarks toolbar collapsed");
  is(bookmarksToolbar.getBoundingClientRect().height, 0, "bookmarksToolbar should have height=0");
  isnot(tabsToolbar.getBoundingClientRect().height, 0, "TabsToolbar should have non-zero height");
  is(navbar.collapsed, false, "The nav-bar should be shown by default");

  setToolbarVisibility(bookmarksToolbar, true);
  setToolbarVisibility(navbar, false);
  isnot(bookmarksToolbar.getBoundingClientRect().height, 0, "bookmarksToolbar should be visible now");
  ok(navbar.getBoundingClientRect().height <= 1, "navbar should have height=0 or 1 (due to border)");
  is(CustomizableUI.inDefaultState, false, "Should no longer be in default state");

  yield startCustomizing();
  gCustomizeMode.reset();
  yield waitForCondition(function() !gCustomizeMode.resetting);
  yield endCustomizing();

  is(bookmarksToolbar.collapsed, true, "Customization reset should restore collapsed-state to the bookmarks toolbar");
  isnot(tabsToolbar.getBoundingClientRect().height, 0, "TabsToolbar should have non-zero height");
  is(bookmarksToolbar.getBoundingClientRect().height, 0, "The bookmarksToolbar should have height=0 after reset");
  ok(CustomizableUI.inDefaultState, "Everything should be back to default state");
});

// Check that the menubar will be collapsed by resetting, if the platform supports it.
add_task(function() {
  let menubar = document.getElementById("toolbar-menubar");
  const canMenubarCollapse = CustomizableUI.isToolbarDefaultCollapsed(menubar.id);
  if (!canMenubarCollapse) {
    return;
  }
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");

  is(menubar.getBoundingClientRect().height, 0, "menubar should be hidden by default");
  setToolbarVisibility(menubar, true);
  isnot(menubar.getBoundingClientRect().height, 0, "menubar should be visible now");

  yield startCustomizing();
  gCustomizeMode.reset();
  yield waitForCondition(function() !gCustomizeMode.resetting);

  is(menubar.getAttribute("autohide"), "true", "The menubar should have autohide=true after reset in customization mode");
  is(menubar.getBoundingClientRect().height, 0, "The menubar should have height=0 after reset in customization mode");

  yield endCustomizing();

  is(menubar.getAttribute("autohide"), "true", "The menubar should have autohide=true after reset");
  is(menubar.getBoundingClientRect().height, 0, "The menubar should have height=0 after reset");
});

// Customization reset should restore collapsed-state to default-collapsed toolbars.
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");
  is(bookmarksToolbar.getBoundingClientRect().height, 0, "bookmarksToolbar should have height=0");
  isnot(tabsToolbar.getBoundingClientRect().height, 0, "TabsToolbar should have non-zero height");

  setToolbarVisibility(bookmarksToolbar, true);
  isnot(bookmarksToolbar.getBoundingClientRect().height, 0, "bookmarksToolbar should be visible now");
  is(CustomizableUI.inDefaultState, false, "Should no longer be in default state");

  yield startCustomizing();

  isnot(bookmarksToolbar.getBoundingClientRect().height, 0, "The bookmarksToolbar should be visible before reset");
  isnot(navbar.getBoundingClientRect().height, 0, "The navbar should be visible before reset");
  isnot(tabsToolbar.getBoundingClientRect().height, 0, "TabsToolbar should have non-zero height");

  gCustomizeMode.reset();
  yield waitForCondition(function() !gCustomizeMode.resetting);

  is(bookmarksToolbar.getBoundingClientRect().height, 0, "The bookmarksToolbar should have height=0 after reset");
  isnot(tabsToolbar.getBoundingClientRect().height, 0, "TabsToolbar should have non-zero height");
  isnot(navbar.getBoundingClientRect().height, 0, "The navbar should still be visible after reset");
  ok(CustomizableUI.inDefaultState, "Everything should be back to default state");
  yield endCustomizing();
});

// Check that the menubar will be collapsed by resetting, if the platform supports it.
add_task(function() {
  let menubar = document.getElementById("toolbar-menubar");
  const canMenubarCollapse = CustomizableUI.isToolbarDefaultCollapsed(menubar.id);
  if (!canMenubarCollapse) {
    return;
  }
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");
  yield startCustomizing();
  let resetButton = document.getElementById("customization-reset-button");
  is(resetButton.disabled, true, "The reset button should be disabled when in default state");

  setToolbarVisibility(menubar, true);
  is(resetButton.disabled, false, "The reset button should be enabled when not in default state")
  ok(!CustomizableUI.inDefaultState, "No longer in default state when the menubar is shown");

  yield gCustomizeMode.reset();

  is(resetButton.disabled, true, "The reset button should be disabled when in default state");
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");

  yield endCustomizing();
});
