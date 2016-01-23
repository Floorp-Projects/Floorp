"use strict";

var overflowPanel = document.getElementById("widget-overflow");

const isOSX = (Services.appinfo.OS === "Darwin");

var originalWindowWidth;
registerCleanupFunction(function() {
  overflowPanel.removeAttribute("animate");
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Right-click on an item within the overflow panel should
// show a context menu with options to move it.
add_task(function*() {

  overflowPanel.setAttribute("animate", "false");

  originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  let oldChildCount = navbar.customizationTarget.childElementCount;
  window.resizeTo(400, window.outerHeight);

  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, overflowPanel);
  chevron.click();
  yield shownPanelPromise;

  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownContextPromise = popupShown(contextMenu);
  let homeButton = document.getElementById("home-button");
  ok(homeButton, "home-button was found");
  is(homeButton.getAttribute("overflowedItem"), "true", "Home button is overflowing");
  EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2});
  yield shownContextPromise;

  is(overflowPanel.state, "open", "The widget overflow panel should still be open.");

  let expectedEntries = [
    [".customize-context-moveToPanel", true],
    [".customize-context-removeFromToolbar", true],
    ["---"]
  ];
  if (!isOSX) {
    expectedEntries.push(["#toggle_toolbar-menubar", true]);
  }
  expectedEntries.push(
    ["#toggle_PersonalToolbar", true],
    ["---"],
    [".viewCustomizeToolbar", true]
  );
  checkContextMenu(contextMenu, expectedEntries);

  let hiddenContextPromise = popupHidden(contextMenu);
  let hiddenPromise = promisePanelElementHidden(window, overflowPanel);
  let moveToPanel = contextMenu.querySelector(".customize-context-moveToPanel");
  if (moveToPanel) {
    moveToPanel.click();
  }
  contextMenu.hidePopup();
  yield hiddenContextPromise;
  yield hiddenPromise;

  let homeButtonPlacement = CustomizableUI.getPlacementOfWidget("home-button");
  ok(homeButtonPlacement, "Home button should still have a placement");
  is(homeButtonPlacement && homeButtonPlacement.area, "PanelUI-contents", "Home button should be in the panel now");
  CustomizableUI.reset();

  // In some cases, it can take a tick for the navbar to overflow again. Wait for it:
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  homeButtonPlacement = CustomizableUI.getPlacementOfWidget("home-button");
  ok(homeButtonPlacement, "Home button should still have a placement");
  is(homeButtonPlacement && homeButtonPlacement.area, "nav-bar", "Home button should be back in the navbar now");

  is(homeButton.getAttribute("overflowedItem"), "true", "Home button should still be overflowed");
});
