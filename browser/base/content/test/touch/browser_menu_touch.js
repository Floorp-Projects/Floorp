/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test checks that toolbar menus are in touchmode
 * when opened through a touch event. */

async function openAndCheckMenu(menu, target) {
  is(menu.state, "closed", "Menu panel is initally closed.");

  let popupshown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeNativeTapAtCenter(target);
  await popupshown;

  is(menu.state, "open", "Menu panel is open.");
  is(menu.getAttribute("touchmode"), "true", "Menu panel is in touchmode.");

  menu.hidePopup();

  popupshown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(target, {});
  await popupshown;

  is(menu.state, "open", "Menu panel is open.");
  ok(!menu.hasAttribute("touchmode"), "Menu panel is not in touchmode.");

  menu.hidePopup();
}

// The customization UI menu is not attached to the document when it is
// closed and hence requires special attention.
async function openAndCheckCustomizationUIMenu(target) {
  EventUtils.synthesizeNativeTapAtCenter(target);

  await BrowserTestUtils.waitForCondition(() =>
      document.getElementById("customizationui-widget-panel") != null);
  let menu = document.getElementById("customizationui-widget-panel");

  if (menu.state != "open") {
    await BrowserTestUtils.waitForEvent(menu, "popupshown");
    is(menu.state, "open", "Menu is open");
  }

  is(menu.getAttribute("touchmode"), "true", "Menu is in touchmode.");

  menu.hidePopup();

  EventUtils.synthesizeMouseAtCenter(target, {});

  await BrowserTestUtils.waitForCondition(() =>
      document.getElementById("customizationui-widget-panel") != null);
  menu = document.getElementById("customizationui-widget-panel");

  if (menu.state != "open") {
    await BrowserTestUtils.waitForEvent(menu, "popupshown");
    is(menu.state, "open", "Menu is open");
  }

  ok(!menu.hasAttribute("touchmode"), "Menu is not in touchmode.");

  menu.hidePopup();
}

// Test main ("hamburger") menu.
add_task(async function test_main_menu_touch() {
  if (!gPhotonStructure) {
    ok(true, "Skipping test because we're not in Photon mode");
    return;
  }

  let mainMenu = document.getElementById("appMenu-popup");
  let target = document.getElementById("PanelUI-menu-button");
  await openAndCheckMenu(mainMenu, target);
});

// Test the page action menu.
add_task(async function test_page_action_panel_touch() {
  if (!gPhotonStructure) {
    ok(true, "Skipping test because we're not in Photon mode");
    return;
  }

  let pageActionPanel = document.getElementById("page-action-panel");
  let target = document.getElementById("urlbar-page-action-button");
  await openAndCheckMenu(pageActionPanel, target);
});

// Test the customizationUI panel, which is used for various menus
// such as library, history, sync, developer and encoding.
add_task(async function test_customizationui_panel_touch() {
  if (!gPhotonStructure) {
    ok(true, "Skipping test because we're not in Photon mode");
    return;
  }

  CustomizableUI.addWidgetToArea("library-button", CustomizableUI.AREA_NAVBAR);
  CustomizableUI.addWidgetToArea("history-panelmenu", CustomizableUI.AREA_NAVBAR);

  await BrowserTestUtils.waitForCondition(() =>
    CustomizableUI.getPlacementOfWidget("library-button").area == "nav-bar");

  let target = document.getElementById("library-button");
  await openAndCheckCustomizationUIMenu(target);

  target = document.getElementById("history-panelmenu");
  await openAndCheckCustomizationUIMenu(target);

  CustomizableUI.reset();
});
