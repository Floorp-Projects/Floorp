/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kAnchorAttribute = "cui-anchorid";

/**
 * Check that anchor gets set correctly when moving an item from the panel to the toolbar
 * using 'undo'
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  await startCustomizing();
  let button = document.getElementById("history-panelmenu");
  is(button.getAttribute(kAnchorAttribute), "PanelUI-menu-button",
     "Button (" + button.id + ") starts out with correct anchor");

  let navbar = document.getElementById("nav-bar").customizationTarget;
  let onMouseUp = BrowserTestUtils.waitForEvent(navbar, "mouseup");
  simulateItemDrag(button, navbar);
  await onMouseUp;
  is(CustomizableUI.getPlacementOfWidget(button.id).area, "nav-bar",
     "Button (" + button.id + ") ends up in nav-bar");

  ok(!button.hasAttribute(kAnchorAttribute),
     "Button (" + button.id + ") has no anchor in toolbar");

  let resetButton = document.getElementById("customization-reset-button");
  ok(!resetButton.hasAttribute("disabled"), "Should be able to reset now.");
  await gCustomizeMode.reset();

  is(button.getAttribute(kAnchorAttribute), "PanelUI-menu-button",
     "Button (" + button.id + ") has anchor again");

  let undoButton = document.getElementById("customization-undo-reset-button");
  ok(!undoButton.hasAttribute("disabled"), "Should be able to undo now.");
  await gCustomizeMode.undoReset();

  ok(!button.hasAttribute(kAnchorAttribute),
     "Button (" + button.id + ") once again has no anchor in toolbar");

  await gCustomizeMode.reset();

  await endCustomizing();
});


/**
 * Check that anchor gets set correctly when moving an item from the panel to the toolbar
 * using 'reset'
 */
add_task(async function() {
  await startCustomizing();
  let button = document.getElementById("bookmarks-menu-button");
  ok(!button.hasAttribute(kAnchorAttribute),
     "Button (" + button.id + ") has no anchor in toolbar");

  let panel = document.getElementById("PanelUI-contents");
  let onMouseUp = BrowserTestUtils.waitForEvent(panel, "mouseup");
  simulateItemDrag(button, panel);
  await onMouseUp;
  is(CustomizableUI.getPlacementOfWidget(button.id).area, "PanelUI-contents",
     "Button (" + button.id + ") ends up in panel");
  is(button.getAttribute(kAnchorAttribute), "PanelUI-menu-button",
     "Button (" + button.id + ") has correct anchor in the panel");

  let resetButton = document.getElementById("customization-reset-button");
  ok(!resetButton.hasAttribute("disabled"), "Should be able to reset now.");
  await gCustomizeMode.reset();

  ok(!button.hasAttribute(kAnchorAttribute),
     "Button (" + button.id + ") once again has no anchor in toolbar");

  await endCustomizing();
});
