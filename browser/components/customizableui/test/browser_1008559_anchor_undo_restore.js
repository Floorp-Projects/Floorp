/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kAnchorAttribute = "cui-anchorid";

/**
 * Check that anchor gets set correctly when moving an item from the panel to the toolbar
 * and into the palette.
 */
add_task(async function() {
  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  await startCustomizing();
  let button = document.getElementById("history-panelmenu");
  is(
    button.getAttribute(kAnchorAttribute),
    "nav-bar-overflow-button",
    "Button (" + button.id + ") starts out with correct anchor"
  );

  let navbar = CustomizableUI.getCustomizationTarget(
    document.getElementById("nav-bar")
  );
  let onMouseUp = BrowserTestUtils.waitForEvent(navbar, "mouseup");
  simulateItemDrag(button, navbar);
  await onMouseUp;
  is(
    CustomizableUI.getPlacementOfWidget(button.id).area,
    "nav-bar",
    "Button (" + button.id + ") ends up in nav-bar"
  );

  ok(
    !button.hasAttribute(kAnchorAttribute),
    "Button (" + button.id + ") has no anchor in toolbar"
  );

  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  is(
    button.getAttribute(kAnchorAttribute),
    "nav-bar-overflow-button",
    "Button (" + button.id + ") has anchor again"
  );

  let resetButton = document.getElementById("customization-reset-button");
  ok(!resetButton.hasAttribute("disabled"), "Should be able to reset now.");
  await gCustomizeMode.reset();

  ok(
    !button.hasAttribute(kAnchorAttribute),
    "Button (" + button.id + ") once again has no anchor in customize panel"
  );

  await endCustomizing();
});

/**
 * Check that anchor gets set correctly when moving an item from the panel to the toolbar
 * using 'reset'
 */
add_task(async function() {
  await startCustomizing();
  let button = document.getElementById("stop-reload-button");
  ok(
    !button.hasAttribute(kAnchorAttribute),
    "Button (" + button.id + ") has no anchor in toolbar"
  );

  let panel = document.getElementById(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  let onMouseUp = BrowserTestUtils.waitForEvent(panel, "mouseup");
  simulateItemDrag(button, panel);
  await onMouseUp;
  is(
    CustomizableUI.getPlacementOfWidget(button.id).area,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL,
    "Button (" + button.id + ") ends up in panel"
  );
  is(
    button.getAttribute(kAnchorAttribute),
    "nav-bar-overflow-button",
    "Button (" + button.id + ") has correct anchor in the panel"
  );

  let resetButton = document.getElementById("customization-reset-button");
  ok(!resetButton.hasAttribute("disabled"), "Should be able to reset now.");
  await gCustomizeMode.reset();

  ok(
    !button.hasAttribute(kAnchorAttribute),
    "Button (" + button.id + ") once again has no anchor in toolbar"
  );

  await endCustomizing();
});
