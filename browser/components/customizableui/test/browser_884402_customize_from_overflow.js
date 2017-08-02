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
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});

  overflowPanel.setAttribute("animate", "false");

  originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  window.resizeTo(400, window.outerHeight);

  await waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, overflowPanel);
  chevron.click();
  await shownPanelPromise;

  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownContextPromise = popupShown(contextMenu);
  let homeButton = document.getElementById("home-button");
  ok(homeButton, "home-button was found");
  is(homeButton.getAttribute("overflowedItem"), "true", "Home button is overflowing");
  EventUtils.synthesizeMouse(homeButton, 2, 2, {type: "contextmenu", button: 2});
  await shownContextPromise;

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
  await hiddenContextPromise;
  await hiddenPromise;

  let homeButtonPlacement = CustomizableUI.getPlacementOfWidget("home-button");
  ok(homeButtonPlacement, "Home button should still have a placement");
  is(homeButtonPlacement && homeButtonPlacement.area, "PanelUI-contents", "Home button should be in the panel now");
  CustomizableUI.reset();

  // In some cases, it can take a tick for the navbar to overflow again. Wait for it:
  await waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  homeButtonPlacement = CustomizableUI.getPlacementOfWidget("home-button");
  ok(homeButtonPlacement, "Home button should still have a placement");
  is(homeButtonPlacement && homeButtonPlacement.area, "nav-bar", "Home button should be back in the navbar now");

  is(homeButton.getAttribute("overflowedItem"), "true", "Home button should still be overflowed");
});
