/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

var bookmarksToolbar = document.getElementById("PersonalToolbar");
var navbar = document.getElementById("nav-bar");
var tabsToolbar = document.getElementById("TabsToolbar");

// Customization reset should restore visibility to default-visible toolbars.
add_task(function*() {
  is(navbar.collapsed, false, "Test should start with navbar visible");
  setToolbarVisibility(navbar, false);
  is(navbar.collapsed, true, "navbar should be hidden now");

  yield resetCustomization();

  is(navbar.collapsed, false, "Customization reset should restore visibility to the navbar");
});

// Customization reset should restore collapsed-state to default-collapsed toolbars.
add_task(function*() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");

  is(bookmarksToolbar.collapsed, true, "Test should start with bookmarks toolbar collapsed");
  ok(bookmarksToolbar.collapsed, "bookmarksToolbar should be collapsed");
  ok(!tabsToolbar.collapsed, "TabsToolbar should not be collapsed");
  is(navbar.collapsed, false, "The nav-bar should be shown by default");

  setToolbarVisibility(bookmarksToolbar, true);
  setToolbarVisibility(navbar, false);
  ok(!bookmarksToolbar.collapsed, "bookmarksToolbar should be visible now");
  ok(navbar.collapsed, "navbar should be collapsed");
  is(CustomizableUI.inDefaultState, false, "Should no longer be in default state");

  yield startCustomizing();
  yield gCustomizeMode.reset();
  yield endCustomizing();

  is(bookmarksToolbar.collapsed, true, "Customization reset should restore collapsed-state to the bookmarks toolbar");
  ok(!tabsToolbar.collapsed, "TabsToolbar should not be collapsed");
  ok(bookmarksToolbar.collapsed, "The bookmarksToolbar should be collapsed after reset");
  ok(CustomizableUI.inDefaultState, "Everything should be back to default state");
});

// Check that the menubar will be collapsed by resetting, if the platform supports it.
add_task(function*() {
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
  yield gCustomizeMode.reset();

  is(menubar.getAttribute("autohide"), "true", "The menubar should have autohide=true after reset in customization mode");
  is(menubar.getBoundingClientRect().height, 0, "The menubar should have height=0 after reset in customization mode");

  yield endCustomizing();

  is(menubar.getAttribute("autohide"), "true", "The menubar should have autohide=true after reset");
  is(menubar.getBoundingClientRect().height, 0, "The menubar should have height=0 after reset");
});

// Customization reset should restore collapsed-state to default-collapsed toolbars.
add_task(function*() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state");
  ok(bookmarksToolbar.collapsed, "bookmarksToolbar should be collapsed");
  ok(!tabsToolbar.collapsed, "TabsToolbar should not be collapsed");

  setToolbarVisibility(bookmarksToolbar, true);
  ok(!bookmarksToolbar.collapsed, "bookmarksToolbar should be visible now");
  is(CustomizableUI.inDefaultState, false, "Should no longer be in default state");

  yield startCustomizing();

  ok(!bookmarksToolbar.collapsed, "The bookmarksToolbar should be visible before reset");
  ok(!navbar.collapsed, "The navbar should be visible before reset");
  ok(!tabsToolbar.collapsed, "TabsToolbar should not be collapsed");

  yield gCustomizeMode.reset();

  ok(bookmarksToolbar.collapsed, "The bookmarksToolbar should be collapsed after reset");
  ok(!tabsToolbar.collapsed, "TabsToolbar should not be collapsed");
  ok(!navbar.collapsed, "The navbar should still be visible after reset");
  ok(CustomizableUI.inDefaultState, "Everything should be back to default state");
  yield endCustomizing();
});

// Check that the menubar will be collapsed by resetting, if the platform supports it.
add_task(function*() {
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
