/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kButton = "test_button_for_addon";
var initialLocation = gBrowser.currentURI.spec;

add_task(async function() {
  info("Check addon button functionality");

  // create mocked addon button on the navigation bar
  let widgetSpec = {
    id: kButton,
    type: "button",
    onClick() {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:addons");
    },
  };
  CustomizableUI.createWidget(widgetSpec);
  CustomizableUI.addWidgetToArea(kButton, CustomizableUI.AREA_NAVBAR);

  // check the button's functionality in navigation bar
  let addonButton = document.getElementById(kButton);
  let navBar = document.getElementById("nav-bar");
  ok(addonButton, "Addon button exists");
  ok(navBar.contains(addonButton), "Addon button is in the navbar");
  await checkButtonFunctionality(addonButton);

  resetTabs();

  // move the add-on button in the Panel Menu
  CustomizableUI.addWidgetToArea(
    kButton,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  ok(
    !navBar.contains(addonButton),
    "Addon button was removed from the browser bar"
  );

  await waitForOverflowButtonShown();

  // check the addon button's functionality in the Panel Menu
  await document.getElementById("nav-bar").overflowable.show();
  var panelMenu = document.getElementById("widget-overflow-mainView");
  let addonButtonInPanel = panelMenu.getElementsByAttribute("id", kButton);
  ok(
    panelMenu.contains(addonButton),
    "Addon button was added to the Panel Menu"
  );
  await checkButtonFunctionality(addonButtonInPanel[0]);
});

add_task(async function asyncCleanup() {
  resetTabs();

  // reset the UI to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The UI is in default state again.");

  // destroy the widget
  CustomizableUI.destroyWidget(kButton);
});

function resetTabs() {
  // close all opened tabs
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.selectedTab);
  }

  // restore the initial tab
  BrowserTestUtils.addTab(gBrowser, initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
}

async function checkButtonFunctionality(aButton) {
  aButton.click();
  await waitForCondition(
    () => gBrowser.currentURI && gBrowser.currentURI.spec == "about:addons"
  );
}
