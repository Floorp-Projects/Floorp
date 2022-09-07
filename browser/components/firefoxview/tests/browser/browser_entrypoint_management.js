/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_removing_button_should_close_tab() {
  await withFirefoxView({}, async browser => {
    let win = browser.ownerGlobal;
    let tab = browser.getTabBrowser().getTabForBrowser(browser);
    let button = win.document.getElementById("firefox-view-button");
    await win.gCustomizeMode.removeFromArea(button, "toolbar-context-menu");
    ok(!tab.isConnected, "Tab should have been removed.");
    isnot(win.gBrowser.selectedTab, tab, "A different tab should be selected.");
  });
  CustomizableUI.reset();
});

add_task(async function test_button_auto_readd() {
  await withFirefoxView({}, async browser => {
    let { FirefoxViewHandler } = browser.ownerGlobal;

    CustomizableUI.removeWidgetFromArea("firefox-view-button");
    ok(
      !CustomizableUI.getPlacementOfWidget("firefox-view-button"),
      "Button has no placement"
    );
    ok(!FirefoxViewHandler.tab, "Shouldn't have tab reference");
    ok(!FirefoxViewHandler.button, "Shouldn't have button reference");

    FirefoxViewHandler.openTab();
    ok(FirefoxViewHandler.tab, "Tab re-opened");
    ok(FirefoxViewHandler.button, "Button re-added");
    let placement = CustomizableUI.getPlacementOfWidget("firefox-view-button");
    is(
      placement.area,
      CustomizableUI.AREA_TABSTRIP,
      "Button re-added to the tabs toolbar"
    );
    is(placement.position, 0, "Button re-added as the first toolbar element");
  });
  CustomizableUI.reset();
});

add_task(async function test_button_moved() {
  await withFirefoxView({}, async browser => {
    let { FirefoxViewHandler } = browser.ownerGlobal;
    CustomizableUI.addWidgetToArea(
      "firefox-view-button",
      CustomizableUI.AREA_NAVBAR,
      0
    );
    is(
      FirefoxViewHandler.button.closest("toolbar").id,
      "nav-bar",
      "Button is in the navigation toolbar"
    );
  });
  await withFirefoxView({}, async browser => {
    let { FirefoxViewHandler } = browser.ownerGlobal;
    is(
      FirefoxViewHandler.button.closest("toolbar").id,
      "nav-bar",
      "Button remains in the navigation toolbar"
    );
  });
  CustomizableUI.reset();
});
