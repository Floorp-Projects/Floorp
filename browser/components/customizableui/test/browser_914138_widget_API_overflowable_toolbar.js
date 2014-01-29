/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
let overflowList = document.getElementById(navbar.getAttribute("overflowtarget"));

const kTestBtn1 = "test-addWidgetToArea-overflow";
const kTestBtn2 = "test-removeWidgetFromArea-overflow";
const kTestBtn3 = "test-createWidget-overflow";
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
  let homeBtnNode = overflowList.querySelector("#" + kHomeBtn);
  ok(homeBtnNode, "Home button should be overflowing");
  ok(homeBtnNode && homeBtnNode.classList.contains("overflowedItem"), "Home button should have overflowedItem class");

  let placementOfHomeButton = CustomizableUI.getWidgetIdsInArea(navbar.id).indexOf(kHomeBtn);
  CustomizableUI.addWidgetToArea(kTestBtn1, navbar.id, placementOfHomeButton);
  ok(!navbar.querySelector("#" + kTestBtn1), "New button should not be in the navbar");
  let newButtonNode = overflowList.querySelector("#" + kTestBtn1);
  ok(newButtonNode, "New button should be overflowing");
  ok(newButtonNode && newButtonNode.classList.contains("overflowedItem"), "New button should have overflowedItem class");
  let nextEl = newButtonNode && newButtonNode.nextSibling;
  is(nextEl && nextEl.id, kHomeBtn, "Test button should be next to home button.");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
  ok(navbar.querySelector("#" + kHomeBtn), "Home button should be in the navbar");
  ok(homeBtnNode && !homeBtnNode.classList.contains("overflowedItem"), "Home button should no longer have overflowedItem class");
  ok(!overflowList.querySelector("#" + kHomeBtn), "Home button should no longer be overflowing");
  ok(navbar.querySelector("#" + kTestBtn1), "Test button should be in the navbar");
  ok(!overflowList.querySelector("#" + kTestBtn1), "Test button should no longer be overflowing");
  ok(newButtonNode && !newButtonNode.classList.contains("overflowedItem"), "New button should no longer have overflowedItem class");
  let el = document.getElementById(kTestBtn1);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn1);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
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

// Constructing a widget while overflown should set the right class on it.
add_task(function() {
  originalWindowWidth = window.outerWidth;
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kHomeBtn), "Home button should no longer be in the navbar");
  let homeBtnNode = overflowList.querySelector("#" + kHomeBtn);
  ok(homeBtnNode, "Home button should be overflowing");
  ok(homeBtnNode && homeBtnNode.classList.contains("overflowedItem"), "Home button should have overflowedItem class");

  let testBtnSpec = {id: kTestBtn3, label: "Overflowable widget test", defaultArea: "nav-bar"};
  CustomizableUI.createWidget(testBtnSpec);
  let testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(testNode && testNode.classList.contains("overflowedItem"), "Test button should have overflowedItem class");

  CustomizableUI.destroyWidget(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");

  CustomizableUI.createWidget(testBtnSpec);
  testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(testNode && testNode.classList.contains("overflowedItem"), "Test button should have overflowedItem class");

  CustomizableUI.removeWidgetFromArea(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");
  CustomizableUI.destroyWidget(kTestBtn3);
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

add_task(function asyncCleanup() {
  window.resizeTo(originalWindowWidth, window.outerHeight);
  yield resetCustomization();
});
