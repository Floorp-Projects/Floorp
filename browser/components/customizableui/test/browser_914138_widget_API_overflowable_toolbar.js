/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
var overflowList = document.getElementById(
  navbar.getAttribute("overflowtarget")
);

const kTestBtn1 = "test-addWidgetToArea-overflow";
const kTestBtn2 = "test-removeWidgetFromArea-overflow";
const kTestBtn3 = "test-createWidget-overflow";
const kTestBtn4 = "test-createWidget-overflow-first-item";
const kTestBtn5 = "test-addWidgetToArea-overflow-first-item";
const kSidebarBtn = "sidebar-button";
const kLibraryButton = "library-button";
const kDownloadsBtn = "downloads-button";
const kSearchBox = "search-container";

var originalWindowWidth;

// Adding a widget should add it next to the widget it's being inserted next to.
add_task(async function subsequent_widget() {
  originalWindowWidth = window.outerWidth;
  createDummyXULButton(kTestBtn1, "Test");
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start subsequent_widget with a non-overflowing toolbar."
  );
  ok(
    CustomizableUI.inDefaultState,
    "Should start subsequent_widget in default state."
  );
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea(kSidebarBtn, "nav-bar");
    await waitForElementShown(document.getElementById(kSidebarBtn));
  }

  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() => {
    return (
      navbar.hasAttribute("overflowing") &&
      document.getElementById(kSidebarBtn).getAttribute("overflowedItem") ==
        "true"
    );
  });
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(
    !navbar.querySelector("#" + kSidebarBtn),
    "Sidebar button should no longer be in the navbar"
  );
  let sidebarBtnNode = overflowList.querySelector("#" + kSidebarBtn);
  ok(sidebarBtnNode, "Sidebar button should be overflowing");
  ok(
    sidebarBtnNode && sidebarBtnNode.getAttribute("overflowedItem") == "true",
    "Sidebar button should have overflowedItem attribute"
  );

  let placementOfSidebarButton = CustomizableUI.getWidgetIdsInArea(
    navbar.id
  ).indexOf(kSidebarBtn);
  CustomizableUI.addWidgetToArea(
    kTestBtn1,
    navbar.id,
    placementOfSidebarButton
  );
  ok(
    !navbar.querySelector("#" + kTestBtn1),
    "New button should not be in the navbar"
  );
  let newButtonNode = overflowList.querySelector("#" + kTestBtn1);
  ok(newButtonNode, "New button should be overflowing");
  ok(
    newButtonNode && newButtonNode.getAttribute("overflowedItem") == "true",
    "New button should have overflowedItem attribute"
  );
  let nextEl = newButtonNode && newButtonNode.nextElementSibling;
  is(
    nextEl && nextEl.id,
    kSidebarBtn,
    "Test button should be next to sidebar button."
  );

  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should not have an overflowing toolbar."
  );
  ok(
    navbar.querySelector("#" + kSidebarBtn),
    "Sidebar button should be in the navbar"
  );
  ok(
    sidebarBtnNode && sidebarBtnNode.getAttribute("overflowedItem") != "true",
    "Sidebar button should no longer have overflowedItem attribute"
  );
  ok(
    !overflowList.querySelector("#" + kSidebarBtn),
    "Sidebar button should no longer be overflowing"
  );
  ok(
    navbar.querySelector("#" + kTestBtn1),
    "Test button should be in the navbar"
  );
  ok(
    !overflowList.querySelector("#" + kTestBtn1),
    "Test button should no longer be overflowing"
  );
  ok(
    newButtonNode && newButtonNode.getAttribute("overflowedItem") != "true",
    "New button should no longer have overflowedItem attribute"
  );
  let el = document.getElementById(kTestBtn1);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn1);
    el.remove();
  }
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.removeWidgetFromArea(kSidebarBtn);
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
});

// Removing a widget should remove it from the overflow list if that is where it is, and update it accordingly.
add_task(async function remove_widget() {
  createDummyXULButton(kTestBtn2, "Test");
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start remove_widget with a non-overflowing toolbar."
  );
  ok(
    CustomizableUI.inDefaultState,
    "Should start remove_widget in default state."
  );
  CustomizableUI.addWidgetToArea(kTestBtn2, navbar.id);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should still have a non-overflowing toolbar."
  );

  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(
    !navbar.querySelector("#" + kTestBtn2),
    "Test button should not be in the navbar"
  );
  ok(
    overflowList.querySelector("#" + kTestBtn2),
    "Test button should be overflowing"
  );

  CustomizableUI.removeWidgetFromArea(kTestBtn2);

  ok(
    !overflowList.querySelector("#" + kTestBtn2),
    "Test button should not be overflowing."
  );
  ok(
    !navbar.querySelector("#" + kTestBtn2),
    "Test button should not be in the navbar"
  );
  ok(
    gNavToolbox.palette.querySelector("#" + kTestBtn2),
    "Test button should be in the palette"
  );

  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should not have an overflowing toolbar."
  );
  let el = document.getElementById(kTestBtn2);
  if (el) {
    CustomizableUI.removeWidgetFromArea(kTestBtn2);
    el.remove();
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
});

// Constructing a widget while overflown should set the right class on it.
add_task(async function construct_widget() {
  originalWindowWidth = window.outerWidth;
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start construct_widget with a non-overflowing toolbar."
  );
  ok(
    CustomizableUI.inDefaultState,
    "Should start construct_widget in default state."
  );

  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea(kSidebarBtn, "nav-bar");
    await waitForElementShown(document.getElementById(kSidebarBtn));
  }

  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() => {
    return (
      navbar.hasAttribute("overflowing") &&
      document.getElementById(kSidebarBtn).getAttribute("overflowedItem") ==
        "true"
    );
  });
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(
    !navbar.querySelector("#" + kSidebarBtn),
    "Sidebar button should no longer be in the navbar"
  );
  let sidebarBtnNode = overflowList.querySelector("#" + kSidebarBtn);
  ok(sidebarBtnNode, "Sidebar button should be overflowing");
  ok(
    sidebarBtnNode && sidebarBtnNode.getAttribute("overflowedItem") == "true",
    "Sidebar button should have overflowedItem class"
  );

  let testBtnSpec = {
    id: kTestBtn3,
    label: "Overflowable widget test",
    defaultArea: "nav-bar",
  };
  CustomizableUI.createWidget(testBtnSpec);
  let testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(
    testNode && testNode.getAttribute("overflowedItem") == "true",
    "Test button should have overflowedItem class"
  );

  CustomizableUI.destroyWidget(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");

  CustomizableUI.createWidget(testBtnSpec);
  testNode = overflowList.querySelector("#" + kTestBtn3);
  ok(testNode, "Test button should be overflowing");
  ok(
    testNode && testNode.getAttribute("overflowedItem") == "true",
    "Test button should have overflowedItem class"
  );

  CustomizableUI.removeWidgetFromArea(kTestBtn3);
  testNode = document.getElementById(kTestBtn3);
  ok(!testNode, "Test button should be gone");
  CustomizableUI.destroyWidget(kTestBtn3);
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.removeWidgetFromArea(kSidebarBtn);
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
});

add_task(async function insertBeforeFirstItemInOverflow() {
  originalWindowWidth = window.outerWidth;

  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start insertBeforeFirstItemInOverflow with a non-overflowing toolbar."
  );
  ok(
    CustomizableUI.inDefaultState,
    "Should start insertBeforeFirstItemInOverflow in default state."
  );

  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea(
      kLibraryButton,
      "nav-bar",
      CustomizableUI.getWidgetIdsInArea("nav-bar").indexOf(kDownloadsBtn)
    );
  }
  let libraryButton = document.getElementById(kLibraryButton);
  await waitForElementShown(libraryButton);
  // Ensure nothing flexes to make the resize predictable:
  navbar
    .querySelectorAll("toolbarspring")
    .forEach(s => CustomizableUI.removeWidgetFromArea(s.id));
  let urlbar = document.getElementById("urlbar-container");
  urlbar.style.minWidth = urlbar.getBoundingClientRect().width + "px";
  // Negative number to make the window smaller by the difference between the left side of
  // the item next to the library button and left side of the hamburger one.
  // The width of the overflow button that needs to appear will then be enough to
  // also hide the library button.
  let resizeWidthToMakeLibraryLast =
    libraryButton.nextElementSibling.getBoundingClientRect().left -
    PanelUI.menuButton.parentNode.getBoundingClientRect().left +
    10; // Leave some margin for the margins between buttons etc.;
  window.resizeBy(resizeWidthToMakeLibraryLast, 0);
  await TestUtils.waitForCondition(() => {
    return (
      libraryButton.getAttribute("overflowedItem") == "true" &&
      !libraryButton.previousElementSibling
    );
  });

  let testBtnSpec = { id: kTestBtn4, label: "Overflowable widget test" };
  let placementOfLibraryButton = CustomizableUI.getWidgetIdsInArea(
    navbar.id
  ).indexOf(kLibraryButton);
  CustomizableUI.createWidget(testBtnSpec);
  CustomizableUI.addWidgetToArea(
    kTestBtn4,
    "nav-bar",
    placementOfLibraryButton
  );
  let testNode = overflowList.querySelector("#" + kTestBtn4);
  ok(testNode, "Test button should be overflowing");
  ok(
    testNode && testNode.getAttribute("overflowedItem") == "true",
    "Test button should have overflowedItem class"
  );
  CustomizableUI.destroyWidget(kTestBtn4);
  testNode = document.getElementById(kTestBtn4);
  ok(!testNode, "Test button should be gone");

  createDummyXULButton(kTestBtn5, "Test");
  CustomizableUI.addWidgetToArea(
    kTestBtn5,
    "nav-bar",
    placementOfLibraryButton
  );
  testNode = overflowList.querySelector("#" + kTestBtn5);
  ok(testNode, "Test button should be overflowing");
  ok(
    testNode && testNode.getAttribute("overflowedItem") == "true",
    "Test button should have overflowedItem class"
  );
  CustomizableUI.removeWidgetFromArea(kTestBtn5);
  testNode && testNode.remove();

  urlbar.style.removeProperty("min-width");
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.removeWidgetFromArea(kLibraryButton);
  }
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
  await resetCustomization();
});

registerCleanupFunction(async function asyncCleanup() {
  document.getElementById("urlbar-container").style.removeProperty("min-width");
  window.resizeTo(originalWindowWidth, window.outerHeight);
  await TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
  await resetCustomization();
});
