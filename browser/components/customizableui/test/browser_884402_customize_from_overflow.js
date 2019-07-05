"use strict";

var overflowPanel = document.getElementById("widget-overflow");

var originalWindowWidth;
registerCleanupFunction(function() {
  overflowPanel.removeAttribute("animate");
  window.resizeTo(originalWindowWidth, window.outerHeight);
});

// Right-click on an item within the overflow panel should
// show a context menu with options to move it.
add_task(async function() {
  overflowPanel.setAttribute("animate", "false");

  originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );
  window.resizeTo(400, window.outerHeight);

  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let chevron = document.getElementById("nav-bar-overflow-button");
  let shownPanelPromise = promisePanelElementShown(window, overflowPanel);
  chevron.click();
  await shownPanelPromise;

  let contextMenu = document.getElementById(
    "customizationPanelItemContextMenu"
  );
  let shownContextPromise = popupShown(contextMenu);
  let sidebarButton = document.getElementById("sidebar-button");
  ok(sidebarButton, "sidebar-button was found");
  is(
    sidebarButton.getAttribute("overflowedItem"),
    "true",
    "Sidebar button is overflowing"
  );
  EventUtils.synthesizeMouse(sidebarButton, 2, 2, {
    type: "contextmenu",
    button: 2,
  });
  await shownContextPromise;

  is(
    overflowPanel.state,
    "open",
    "The widget overflow panel should still be open."
  );

  let expectedEntries = [
    [".customize-context-moveToPanel", true],
    [".customize-context-removeFromPanel", true],
    ["---"],
    [".viewCustomizeToolbar", true],
  ];
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

  let sidebarButtonPlacement = CustomizableUI.getPlacementOfWidget(
    "sidebar-button"
  );
  ok(sidebarButtonPlacement, "Sidebar button should still have a placement");
  is(
    sidebarButtonPlacement && sidebarButtonPlacement.area,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    "Sidebar button should be pinned now"
  );
  CustomizableUI.reset();

  // In some cases, it can take a tick for the navbar to overflow again. Wait for it:
  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  sidebarButtonPlacement = CustomizableUI.getPlacementOfWidget(
    "sidebar-button"
  );
  ok(sidebarButtonPlacement, "Sidebar button should still have a placement");
  is(
    sidebarButtonPlacement && sidebarButtonPlacement.area,
    "nav-bar",
    "Sidebar button should be back in the navbar now"
  );

  is(
    sidebarButton.getAttribute("overflowedItem"),
    "true",
    "Sidebar button should still be overflowed"
  );
});
