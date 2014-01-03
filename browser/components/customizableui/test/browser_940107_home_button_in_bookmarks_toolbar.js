/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 940107 - Home icon not displayed correctly when in bookmarks toolbar.
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Should be in default state when test starts.");
  let bookmarksToolbar = document.getElementById(CustomizableUI.AREA_BOOKMARKS);
  bookmarksToolbar.collapsed = false;

  let homeButton = document.getElementById("home-button");
  ok(homeButton.classList.contains("toolbarbutton-1"), "Home Button should have toolbarbutton-1 when in the nav-bar");
  ok(!homeButton.classList.contains("bookmark-item"), "Home Button should not be displayed as a bookmarks item");

  yield startCustomizing();
  CustomizableUI.addWidgetToArea(homeButton.id, CustomizableUI.AREA_BOOKMARKS);
  yield endCustomizing();
  ok(homeButton.classList.contains("bookmark-item"), "Home Button should be displayed as a bookmarks item");
  ok(!homeButton.classList.contains("toolbarbutton-1"), "Home Button should not be displayed as a nav-bar item");

  gCustomizeMode.addToPanel(homeButton);
  let panelShownPromise = promisePanelShown(window);
  PanelUI.toggle();
  yield panelShownPromise;

  ok(homeButton.classList.contains("toolbarbutton-1"), "Home Button should have toolbarbutton-1 when in the panel");
  ok(!homeButton.classList.contains("bookmark-item"), "Home Button should not be displayed as a bookmarks item");

  gCustomizeMode.addToToolbar(homeButton);
  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.toggle();
  yield panelHiddenPromise;

  ok(homeButton.classList.contains("toolbarbutton-1"), "Home Button should have toolbarbutton-1 when in the nav-bar");
  ok(!homeButton.classList.contains("bookmark-item"), "Home Button should not be displayed as a bookmarks item");

  bookmarksToolbar.collapsed = true;
  CustomizableUI.reset();
});
