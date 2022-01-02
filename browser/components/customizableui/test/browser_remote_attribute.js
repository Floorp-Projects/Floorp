/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * These tests check that the remote attribute is true for remote panels.
 * This attribute is needed for Mac to properly render the panel.
 */
add_task(async function check_remote_attribute() {
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  //let pocketPanelShown = BrowserTestUtils.waitForEvent(
  //  document,
  //  "popupshown",
  //  true
  //);
  let pocketPanelShown = popupShown(document);
  // Using Pocket panel as it's an available remote panel.
  let pocketButton = document.getElementById("save-to-pocket-button");
  pocketButton.click();
  await pocketPanelShown;

  let pocketPanel = document.getElementById("customizationui-widget-panel");
  is(
    pocketPanel.getAttribute("remote"),
    "true",
    "Pocket panel has remote attribute"
  );

  // Close panel and cleanup.
  let pocketPanelHidden = popupHidden(pocketPanel);
  pocketPanel.hidePopup();
  await pocketPanelHidden;
});

add_task(async function check_remote_attribute_overflow() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let overflowPanel = win.document.getElementById("widget-overflow");
  overflowPanel.setAttribute("animate", "false");

  // Force a narrow window to get an overflow toolbar.
  win.resizeTo(kForceOverflowWidthPx, win.outerHeight);
  let navbar = win.document.getElementById(CustomizableUI.AREA_NAVBAR);
  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));

  // Open the overflow panel view.
  let overflowPanelShown = popupShown(overflowPanel);
  let overflowPanelButton = win.document.getElementById(
    "nav-bar-overflow-button"
  );
  overflowPanelButton.click();
  await overflowPanelShown;

  // Using Pocket panel as it's an available remote panel.
  let pocketButton = win.document.getElementById("save-to-pocket-button");
  pocketButton.click();
  await BrowserTestUtils.waitForEvent(win.document, "ViewShown");

  is(
    overflowPanel.getAttribute("remote"),
    "true",
    "Pocket overflow panel has remote attribute"
  );

  // Close panel and cleanup.
  let overflowPanelHidden = popupHidden(overflowPanel);
  overflowPanel.hidePopup();
  await overflowPanelHidden;
  overflowPanel.removeAttribute("animate");
  await BrowserTestUtils.closeWindow(win);
});
