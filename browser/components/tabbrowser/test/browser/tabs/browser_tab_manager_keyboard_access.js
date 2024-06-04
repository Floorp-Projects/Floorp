/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function waitForAllTabsMenu(window = window) {
  // Borrowed from browser_menu_touch.js,
  // Ensure menu has been added to the document and that it's open
  await BrowserTestUtils.waitForCondition(
    () => window.document.getElementById("customizationui-widget-panel") != null
  );
  let menu = window.document.getElementById("customizationui-widget-panel");

  if (menu.state != "open") {
    await BrowserTestUtils.waitForEvent(menu, "popupshown");
    is(menu.state, "open", `All tabs menu is open`);
  }

  return menu;
}

/**
 * Check we can open the tab manager using the keyboard.
 * Note that navigation to buttons in the toolbar is covered
 * by other tests.
 */
add_task(async function test_open_tabmanager_keyboard() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.tabmanager.enabled", true]],
  });
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  let elem = newWindow.document.getElementById("alltabs-button");

  // Borrowed from forceFocus() in the keyboard directory head.js
  elem.setAttribute("tabindex", "-1");
  elem.focus();

  let focused = BrowserTestUtils.waitForEvent(newWindow, "focus", true);
  EventUtils.synthesizeKey(" ", {}, newWindow);
  let event = await focused;
  let allTabsMenu = await waitForAllTabsMenu(newWindow);

  elem.removeAttribute("tabindex");

  ok(
    event.originalTarget.closest("#allTabsMenu-allTabsView"),
    "Focus inside all tabs menu after toolbar button pressed"
  );
  let hidden = BrowserTestUtils.waitForEvent(allTabsMenu, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape", { shiftKey: false }, newWindow);
  await hidden;
  await BrowserTestUtils.closeWindow(newWindow);
});
