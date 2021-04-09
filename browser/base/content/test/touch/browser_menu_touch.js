/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test checks that toolbar menus are in touchmode
 * when opened through a touch event. */

async function openAndCheckMenu(menu, target) {
  is(menu.state, "closed", `Menu panel (${menu.id}) is initally closed.`);

  let popupshown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeNativeTapAtCenter(target);
  await popupshown;

  is(menu.state, "open", `Menu panel (${menu.id}) is open.`);
  is(
    menu.getAttribute("touchmode"),
    "true",
    `Menu panel (${menu.id}) is in touchmode.`
  );

  menu.hidePopup();

  popupshown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(target, {});
  await popupshown;

  is(menu.state, "open", `Menu panel (${menu.id}) is open.`);
  ok(
    !menu.hasAttribute("touchmode"),
    `Menu panel (${menu.id}) is not in touchmode.`
  );

  menu.hidePopup();
}

async function openAndCheckLazyMenu(id, target) {
  let menu = document.getElementById(id);

  EventUtils.synthesizeNativeTapAtCenter(target);
  let ev = await BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    e => e.target.id == id
  );
  menu = ev.target;

  is(menu.state, "open", `Menu panel (${menu.id}) is open.`);
  is(
    menu.getAttribute("touchmode"),
    "true",
    `Menu panel (${menu.id}) is in touchmode.`
  );

  menu.hidePopup();

  let popupshown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(target, {});
  await popupshown;

  is(menu.state, "open", `Menu panel (${menu.id}) is open.`);
  ok(
    !menu.hasAttribute("touchmode"),
    `Menu panel (${menu.id}) is not in touchmode.`
  );

  menu.hidePopup();
}

// The customization UI menu is not attached to the document when it is
// closed and hence requires special attention.
async function openAndCheckCustomizationUIMenu(target) {
  EventUtils.synthesizeNativeTapAtCenter(target);

  await BrowserTestUtils.waitForCondition(
    () => document.getElementById("customizationui-widget-panel") != null
  );
  let menu = document.getElementById("customizationui-widget-panel");

  if (menu.state != "open") {
    await BrowserTestUtils.waitForEvent(menu, "popupshown");
    is(menu.state, "open", `Menu for ${target.id} is open`);
  }

  is(
    menu.getAttribute("touchmode"),
    "true",
    `Menu for ${target.id} is in touchmode.`
  );

  menu.hidePopup();

  EventUtils.synthesizeMouseAtCenter(target, {});

  await BrowserTestUtils.waitForCondition(
    () => document.getElementById("customizationui-widget-panel") != null
  );
  menu = document.getElementById("customizationui-widget-panel");

  if (menu.state != "open") {
    await BrowserTestUtils.waitForEvent(menu, "popupshown");
    is(menu.state, "open", `Menu for ${target.id} is open`);
  }

  ok(
    !menu.hasAttribute("touchmode"),
    `Menu for ${target.id} is not in touchmode.`
  );

  menu.hidePopup();
}

// Ensure that we can run touch events properly for windows [10]
add_task(async function setup() {
  let isWindows = AppConstants.isPlatformAndVersionAtLeast("win", "10.0");
  await SpecialPowers.pushPrefEnv({
    set: [["apz.test.fails_with_native_injection", isWindows]],
  });
});

// Test main ("hamburger") menu.
add_task(async function test_main_menu_touch() {
  let mainMenu = document.getElementById("appMenu-popup");
  let target = document.getElementById("PanelUI-menu-button");
  await openAndCheckMenu(mainMenu, target);
});

// Test the page action menu.
add_task(async function test_page_action_panel_touch() {
  // The page action menu only appears on a web page.
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    // In Proton the page actions button is not normally visible, so we must
    // unhide it.
    if (gProton) {
      BrowserPageActions.mainButtonNode.style.visibility = "visible";
      registerCleanupFunction(() => {
        BrowserPageActions.mainButtonNode.style.removeProperty("visibility");
      });
    }
    let target = document.getElementById("pageActionButton");
    await openAndCheckLazyMenu("pageActionPanel", target);
  });
});

// Test the customizationUI panel, which is used for various menus
// such as library, history, sync, developer and encoding.
add_task(async function test_customizationui_panel_touch() {
  CustomizableUI.addWidgetToArea("library-button", CustomizableUI.AREA_NAVBAR);
  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_NAVBAR
  );

  await BrowserTestUtils.waitForCondition(
    () =>
      CustomizableUI.getPlacementOfWidget("library-button").area == "nav-bar"
  );

  let target = document.getElementById("library-button");
  await openAndCheckCustomizationUIMenu(target);

  target = document.getElementById("history-panelmenu");
  await openAndCheckCustomizationUIMenu(target);

  CustomizableUI.reset();
});

// Test the overflow menu panel.
add_task(async function test_overflow_panel_touch() {
  // Move something in the overflow menu to make the button appear.
  CustomizableUI.addWidgetToArea(
    "library-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  await BrowserTestUtils.waitForCondition(
    () =>
      CustomizableUI.getPlacementOfWidget("library-button").area ==
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  let overflowPanel = document.getElementById("widget-overflow");
  let target = document.getElementById("nav-bar-overflow-button");
  await openAndCheckMenu(overflowPanel, target);

  CustomizableUI.reset();
});

// Test the list all tabs menu.
add_task(async function test_list_all_tabs_touch() {
  // Force the menu button to be shown.
  let tabs = document.getElementById("tabbrowser-tabs");
  if (!tabs.hasAttribute("overflow")) {
    tabs.setAttribute("overflow", true);
    registerCleanupFunction(() => tabs.removeAttribute("overflow"));
  }

  let target = document.getElementById("alltabs-button");
  await openAndCheckCustomizationUIMenu(target);
});
