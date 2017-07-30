/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
var overflowList = document.getElementById(navbar.getAttribute("overflowtarget"));

const kTestBtn1 = "test-addWidgetToArea-overflow";
const kTestBtn2 = "test-removeWidgetFromArea-overflow";
const kTestBtn3 = "test-createWidget-overflow";
const kSidebarBtn = "sidebar-button";
const kDownloadsBtn = "downloads-button";
const kSearchBox = "search-container";

var originalWindowWidth;

// Adding a widget should add it next to the widget it's being inserted next to.
add_task(async function() {
  originalWindowWidth = window.outerWidth;
  createDummyXULButton(kTestBtn1, "Test");
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(400, window.outerHeight);
  await waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kSidebarBtn), "Sidebar button should no longer be in the navbar");
  let sidebarBtnNode = overflowList.querySelector("#" + kSidebarBtn);
  ok(sidebarBtnNode, "Sidebar button should be overflowing");
  ok(sidebarBtnNode && sidebarBtnNode.getAttribute("overflowedItem") == "true", "Sidebar button should have overflowedItem attribute");

  let placementOfSidebarButton = CustomizableUI.getWidgetIdsInArea(navbar.id).indexOf(kSidebarBtn);
  CustomizableUI.addWidgetToArea(kTestBtn1, navbar.id, placementOfSidebarButton);
  ok(!navbar.querySelector("#" + kTestBtn1), "New button should not be in the navbar");
  let newButtonNode = overflowList.querySelector("#" + kTestBtn1);
  ok(newButtonNode, "New button should be overflowing");
  ok(newButtonNode && newButtonNode.getAttribute("overflowedItem") == "true", "New button should have overflowedItem attribute");
  let nextEl = newButtonNode && newButtonNode.nextSibling;
  is(nextEl && nextEl.id, kSidebarBtn, "Test button should be next to sidebar button.");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  await waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
  ok(navbar.querySelector("#" + kSidebarBtn), "Sidebar button should be in the navbar");
  ok(sidebarBtnNode && (sidebarBtnNode.getAttribute("overflowedItem") != "true"), "Sidebar button should no longer have overflowedItem attribute");
  ok(!overflowList.querySelector("#" + kSidebarBtn), "Sidebar button should no longer be overflowing");
  ok(navbar.querySelector("#" + kTestBtn1), "Test button should be in the navbar");
  ok(!overflowList.querySelector("#" + kTestBtn1), "Test button should no longer be overflowing");
  ok(newButtonNode && (newButtonNode.getAttribute("overflowedItem") != "true"), "New button should no longer have overflowedItem attribute");
  let el = document.getElementById(kTestBtn1);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn1);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Removing a widget should remove it from the overflow list if that is where it is, and update it accordingly.
add_task(async function() {
  createDummyXULButton(kTestBtn2, "Test");
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  CustomizableUI.addWidgetToArea(kTestBtn2, navbar.id);
  ok(!navbar.hasAttribute("overflowing"), "Should still have a non-overflowing toolbar.");

  window.resizeTo(400, window.outerHeight);
  await waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kTestBtn2), "Test button should not be in the navbar");
  ok(overflowList.querySelector("#" + kTestBtn2), "Test button should be overflowing");

  CustomizableUI.removeWidgetFromArea(kTestBtn2);

  ok(!overflowList.querySelector("#" + kTestBtn2), "Test button should not be overflowing.");
  ok(!navbar.querySelector("#" + kTestBtn2), "Test button should not be in the navbar");
  ok(gNavToolbox.palette.querySelector("#" + kTestBtn2), "Test button should be in the palette");

  window.resizeTo(originalWindowWidth, window.outerHeight);
  await waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
  let el = document.getElementById(kTestBtn2);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn2);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Constructing a widget while overflown should set the right class on it.
add_task(async function() {
  originalWindowWidth = window.outerWidth;
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(400, window.outerHeight);
  await waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(!navbar.querySelector("#" + kSidebarBtn), "Sidebar button should no longer be in the navbar");
  let sidebarBtnNode = overflowList.querySelector("#" + kSidebarBtn);
  ok(sidebarBtnNode, "Sidebar button should be overflowing");
  ok(sidebarBtnNode && sidebarBtnNode.getAttribute("overflowedItem") == "true", "Sidebar button should have overflowedItem class");

  let testBtnSpec = {id: kTestBtn3, label: "Overflowable widget test", defaultArea: "nav-bar"};
  CustomizableUI.createWidget(testBtnSpec);
  let testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(testNode && testNode.getAttribute("overflowedItem") == "true", "Test button should have overflowedItem class");

  CustomizableUI.destroyWidget(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");

  CustomizableUI.createWidget(testBtnSpec);
  testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(testNode && testNode.getAttribute("overflowedItem") == "true", "Test button should have overflowedItem class");

  CustomizableUI.removeWidgetFromArea(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");
  CustomizableUI.destroyWidget(kTestBtn3);
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

add_task(async function asyncCleanup() {
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await resetCustomization();
});
