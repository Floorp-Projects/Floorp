/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kButton = "test_button_for_addon";
var initialLocation = gBrowser.currentURI.spec;

add_task(function*() {
  info("Check addon button functionality");

  // create mocked addon button on the navigation bar
  let widgetSpec = {
    id: kButton,
    type: 'button',
    onClick: function() {
      gBrowser.selectedTab = gBrowser.addTab("about:addons");
    }
  };
  CustomizableUI.createWidget(widgetSpec);
  CustomizableUI.addWidgetToArea(kButton, CustomizableUI.AREA_NAVBAR);

  // check the button's functionality in navigation bar
  let addonButton = document.getElementById(kButton);
  let navBar = document.getElementById("nav-bar");
  ok(addonButton, "Addon button exists");
  ok(navBar.contains(addonButton), "Addon button is in the navbar");
  yield checkButtonFunctionality(addonButton);

  resetTabs();

  // move the add-on button in the Panel Menu
  CustomizableUI.addWidgetToArea(kButton, CustomizableUI.AREA_PANEL);
  let addonButtonInNavbar = navBar.getElementsByAttribute("id", kButton);
  ok(!navBar.contains(addonButton), "Addon button was removed from the browser bar");

  // check the addon button's functionality in the Panel Menu
  yield PanelUI.show();
  var panelMenu = document.getElementById("PanelUI-mainView");
  let addonButtonInPanel = panelMenu.getElementsByAttribute("id", kButton);
  ok(panelMenu.contains(addonButton), "Addon button was added to the Panel Menu");
  yield checkButtonFunctionality(addonButtonInPanel[0]);
});

add_task(function* asyncCleanup() {
  resetTabs();

  // reset the UI to the default state
  yield resetCustomization();
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
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
}

function* checkButtonFunctionality(aButton) {
  aButton.click();
  yield waitForCondition(() => gBrowser.currentURI &&
                               gBrowser.currentURI.spec == "about:addons");
}
