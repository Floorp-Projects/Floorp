"use strict";

var overflowPanel = document.getElementById("widget-overflow");

var originalWindowWidth;
registerCleanupFunction(function () {
  overflowPanel.removeAttribute("animate");
  window.resizeTo(originalWindowWidth, window.outerHeight);
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  return TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
});

// Right-click on an item within the overflow panel should
// show a context menu with options to move it.
add_task(async function () {
  overflowPanel.setAttribute("animate", "false");
  let fxaButton = document.getElementById("fxa-toolbar-menu-button");
  if (BrowserTestUtils.isHidden(fxaButton)) {
    // FxA button is likely hidden since the user is logged out.
    let initialFxaStatus = document.documentElement.getAttribute("fxastatus");
    document.documentElement.setAttribute("fxastatus", "signed_in");
    registerCleanupFunction(() =>
      document.documentElement.setAttribute("fxastatus", initialFxaStatus)
    );
    ok(BrowserTestUtils.isVisible(fxaButton), "FxA button is now visible");
  }

  originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);

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
  ok(fxaButton, "fxa-toolbar-menu-button was found");
  is(
    fxaButton.getAttribute("overflowedItem"),
    "true",
    "FxA button is overflowing"
  );
  EventUtils.synthesizeMouse(fxaButton, 2, 2, {
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
    contextMenu.activateItem(moveToPanel);
  } else {
    contextMenu.hidePopup();
  }
  await hiddenContextPromise;
  await hiddenPromise;

  let fxaButtonPlacement = CustomizableUI.getPlacementOfWidget(
    "fxa-toolbar-menu-button"
  );
  ok(fxaButtonPlacement, "FxA button should still have a placement");
  is(
    fxaButtonPlacement && fxaButtonPlacement.area,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    "FxA button should be pinned now"
  );
  CustomizableUI.reset();

  // In some cases, it can take a tick for the navbar to overflow again. Wait for it:
  await TestUtils.waitForCondition(() =>
    fxaButton.hasAttribute("overflowedItem")
  );
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  fxaButtonPlacement = CustomizableUI.getPlacementOfWidget(
    "fxa-toolbar-menu-button"
  );
  ok(fxaButtonPlacement, "FxA button should still have a placement");
  is(
    fxaButtonPlacement && fxaButtonPlacement.area,
    "nav-bar",
    "FxA button should be back in the navbar now"
  );

  is(
    fxaButton.getAttribute("overflowedItem"),
    "true",
    "FxA button should still be overflowed"
  );
});
