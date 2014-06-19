/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let originalWindowWidth;

// Drag to overflow chevron should open the overflow panel.
add_task(function*() {
  originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  let oldChildCount = navbar.customizationTarget.childElementCount;
  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  let widgetOverflowPanel = document.getElementById("widget-overflow");
  let panelShownPromise = promisePanelElementShown(window, widgetOverflowPanel);
  let identityBox = document.getElementById("identity-box");
  let overflowChevron = document.getElementById("nav-bar-overflow-button");

  // Listen for hiding immediately so we don't miss the event because of the
  // async-ness of the 'shown' yield...
  let panelHiddenPromise = promisePanelElementHidden(window, widgetOverflowPanel);

  ChromeUtils.synthesizeDrop(identityBox, overflowChevron, [], null);
  yield panelShownPromise;

  info("Overflow panel is shown.");

  widgetOverflowPanel.hidePopup();
  yield panelHiddenPromise;
});

add_task(function*() {
  window.resizeTo(originalWindowWidth, window.outerHeight);
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
});
