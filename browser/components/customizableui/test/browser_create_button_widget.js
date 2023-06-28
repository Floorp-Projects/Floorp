/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kButton = "test_dynamically_created_button";
var initialLocation = gBrowser.currentURI.spec;

add_task(async function () {
  info("Check dynamically created button functionality");

  // Let's create a simple button that will open about:addons.
  let widgetSpec = {
    id: kButton,
    type: "button",
    onClick() {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:addons");
    },
  };
  CustomizableUI.createWidget(widgetSpec);
  CustomizableUI.addWidgetToArea(kButton, CustomizableUI.AREA_NAVBAR);
  ok(
    !CustomizableUI.isWebExtensionWidget(kButton),
    "This button should not be considered an extension widget."
  );

  // check the button's functionality in navigation bar
  let button = document.getElementById(kButton);
  let navBar = document.getElementById("nav-bar");
  ok(button, "Dynamically created button exists");
  ok(navBar.contains(button), "Dynamically created button is in the navbar");
  await checkButtonFunctionality(button);

  resetTabs();

  // move the add-on button in the Panel Menu
  CustomizableUI.addWidgetToArea(
    kButton,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  ok(
    !navBar.contains(button),
    "Dynamically created button was removed from the browser bar"
  );

  await waitForOverflowButtonShown();

  // check the button's functionality in the Overflow Panel.
  await document.getElementById("nav-bar").overflowable.show();
  var panelMenu = document.getElementById("widget-overflow-mainView");
  let buttonInPanel = panelMenu.getElementsByAttribute("id", kButton);
  ok(
    panelMenu.contains(button),
    "Dynamically created button was added to the Panel Menu"
  );
  await checkButtonFunctionality(buttonInPanel[0]);
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
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:addons"
  );
}
