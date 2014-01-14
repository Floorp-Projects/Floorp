/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
let overflowList = document.getElementById(navbar.getAttribute("overflowtarget"));

const kTestBtn1 = "test-addWidgetToArea-overflow";
const kTestBtn2 = "test-removeWidgetFromArea-overflow";
const kHomeBtn = "home-button";
const kDownloadsBtn = "downloads-button";
const kSearchBox = "search-container";
const kStarBtn = "bookmarks-menu-button";

let originalWindowWidth;

// Adding a widget should add it next to the widget it's being inserted next to.
add_task(function() {
  originalWindowWidth = window.outerWidth;
  createDummyXULButton(kTestBtn1, "Test");
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kHomeBtn), "Home button should no longer be in the navbar");
  ok(overflowList.querySelector("#" + kHomeBtn), "Home button should be overflowing");

  let placementOfHomeButton = CustomizableUI.getWidgetIdsInArea(navbar.id).indexOf(kHomeBtn);
  CustomizableUI.addWidgetToArea(kTestBtn1, navbar.id, placementOfHomeButton);
  ok(!navbar.querySelector("#" + kTestBtn1), "New button should not be in the navbar");
  ok(overflowList.querySelector("#" + kTestBtn1), "New button should be overflowing");
  let nextEl = document.getElementById(kTestBtn1).nextSibling;
  is(nextEl && nextEl.id, kHomeBtn, "Test button should be next to home button.");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
  ok(navbar.querySelector("#" + kHomeBtn), "Home button should be in the navbar");
  ok(!overflowList.querySelector("#" + kHomeBtn), "Home button should no longer be overflowing");
  ok(navbar.querySelector("#" + kTestBtn1), "Test button should be in the navbar");
  ok(!overflowList.querySelector("#" + kTestBtn1), "Test button should no longer be overflowing");
  let el = document.getElementById(kTestBtn1);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn1);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Removing a widget from the toolbar should try to move items back.
add_task(function() {
  // This is pretty weird. We're going to try to move only the home button into the overlay:
  let downloadsBtn = document.getElementById(kDownloadsBtn);
  // Guarantee overflow of too much stuff:
  window.resizeTo(700, window.outerHeight);
  let inc = 15;
  while (window.outerWidth < originalWindowWidth &&
         downloadsBtn.parentNode != navbar.customizationTarget) {
    window.resizeTo(window.outerWidth + inc, window.outerHeight);
    yield waitFor(500);
  }
  ok(overflowList.querySelector("#home-button"), "Home button should be overflowing");
  CustomizableUI.removeWidgetFromArea("downloads-button");
  is(document.getElementById("home-button").parentNode, navbar.customizationTarget, "Home button should move back.");
  ok(!navbar.hasAttribute("overflowing"), "Navbar is no longer overflowing");
  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(function() !navbar.hasAttribute("overflowing"));
  CustomizableUI.reset();
});

// Removing a widget should remove it from the overflow list if that is where it is, and update it accordingly.
add_task(function() {
  createDummyXULButton(kTestBtn2, "Test");
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  CustomizableUI.addWidgetToArea(kTestBtn2, navbar.id);
  ok(!navbar.hasAttribute("overflowing"), "Should still have a non-overflowing toolbar.");

  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kTestBtn2), "Test button should not be in the navbar");
  ok(overflowList.querySelector("#" + kTestBtn2), "Test button should be overflowing");

  CustomizableUI.removeWidgetFromArea(kTestBtn2);

  ok(!overflowList.querySelector("#" + kTestBtn2), "Test button should not be overflowing.");
  ok(!navbar.querySelector("#" + kTestBtn2), "Test button should not be in the navbar");
  ok(gNavToolbox.palette.querySelector("#" + kTestBtn2), "Test button should be in the palette");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
  let el = document.getElementById(kTestBtn2);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn2);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Overflow everything that can, then reorganize that list
add_task(function() {
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(380, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(!navbar.querySelector("#" + kSearchBox), "Search container should be overflowing");
  let placements = CustomizableUI.getWidgetIdsInArea(navbar.id);
  let searchboxPlacement = placements.indexOf(kSearchBox);
  CustomizableUI.moveWidgetWithinArea(kHomeBtn, searchboxPlacement);
  yield waitForCondition(() => navbar.querySelector("#" + kHomeBtn));
  ok(navbar.querySelector("#" + kHomeBtn), "Home button should have moved back");
  let inc = 15;
  window.resizeTo(640, window.outerHeight);
  while (window.outerWidth < originalWindowWidth &&
         !navbar.querySelector("#" + kSearchBox)) {
    window.resizeTo(window.outerWidth + inc, window.outerHeight);
    yield waitFor(500);
  }
  ok(!navbar.querySelector("#" + kStarBtn), "Star button should still be overflowed");
  CustomizableUI.moveWidgetWithinArea(kStarBtn);
  let starButtonOverflowed = overflowList.querySelector("#" + kStarBtn);
  ok(starButtonOverflowed && !starButtonOverflowed.nextSibling, "Star button should be last item");
  window.resizeTo(window.outerWidth + 15, window.outerHeight);
  yield waitForCondition(() => navbar.querySelector("#" + kDownloadsBtn) && navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "navbar should still be overflowing");
  CustomizableUI.moveWidgetWithinArea(kHomeBtn);
  let homeButtonOverflowed = overflowList.querySelector("#" + kHomeBtn);
  ok(homeButtonOverflowed, "Home button should be in overflow list");
  ok(navbar.hasAttribute("overflowing"), "navbar should still be overflowing");
  ok(homeButtonOverflowed && !homeButtonOverflowed.nextSibling, "Home button should be last item");
  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
});

add_task(function asyncCleanup() {
  yield resetCustomization();
});
