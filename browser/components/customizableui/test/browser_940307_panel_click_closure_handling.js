/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var button, menuButton;
/* Clicking a button should close the panel */
add_task(async function plain_button() {
  button = document.createElement("toolbarbutton");
  button.id = "browser_940307_button";
  button.setAttribute("label", "Button");
  gNavToolbox.palette.appendChild(button);
  CustomizableUI.addWidgetToArea(button.id, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  let hiddenAgain = promiseOverflowHidden(window);
  EventUtils.synthesizeMouseAtCenter(button, {});
  await hiddenAgain;
  CustomizableUI.removeWidgetFromArea(button.id);
  button.remove();
});

/* Clicking a menu button should close the panel, opening the popup shouldn't.  */
add_task(async function menu_button_popup() {
  menuButton = document.createElement("toolbarbutton");
  menuButton.setAttribute("type", "menu-button");
  menuButton.id = "browser_940307_menubutton";
  menuButton.setAttribute("label", "Menu button");

  let menuPopup = document.createElement("menupopup");
  menuPopup.id = "browser_940307_menupopup";

  let menuItem = document.createElement("menuitem");
  menuItem.setAttribute("label", "Menu item");
  menuItem.id = "browser_940307_menuitem";

  menuPopup.appendChild(menuItem);
  menuButton.appendChild(menuPopup);
  gNavToolbox.palette.appendChild(menuButton);
  CustomizableUI.addWidgetToArea(menuButton.id, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  let hiddenAgain = promiseOverflowHidden(window);
  let innerButton = document.getAnonymousElementByAttribute(menuButton, "anonid", "button");
  EventUtils.synthesizeMouseAtCenter(innerButton, {});
  await hiddenAgain;

  // Now click the dropmarker to show the menu
  await document.getElementById("nav-bar").overflowable.show();
  hiddenAgain = promiseOverflowHidden(window);
  let menuShown = promisePanelElementShown(window, menuPopup);
  let dropmarker = document.getAnonymousElementByAttribute(menuButton, "type", "menu-button");
  EventUtils.synthesizeMouseAtCenter(dropmarker, {});
  await menuShown;
  // Panel should stay open:
  ok(isOverflowOpen(), "Panel should still be open");
  let menuHidden = promisePanelElementHidden(window, menuPopup);
  // Then click the menu item to close all the things
  EventUtils.synthesizeMouseAtCenter(menuItem, {});
  await menuHidden;
  await hiddenAgain;
  CustomizableUI.removeWidgetFromArea(menuButton.id);
  menuButton.remove();
});

add_task(async function searchbar_in_panel() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, "Should be in panel");

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  await waitForCondition(() => "value" in searchbar && searchbar.value === "");

  // Focusing a non-empty searchbox will cause us to open the
  // autocomplete panel and search for suggestions, which would
  // trigger network requests. Temporarily disable suggestions.
  await SpecialPowers.pushPrefEnv({set: [["browser.search.suggest.enabled", false]]});

  searchbar.value = "foo";
  searchbar.focus();
  // Reaching into this context menu is pretty evil, but hey... it's a test.
  let textbox = document.getAnonymousElementByAttribute(searchbar.textbox, "anonid", "textbox-input-box");
  let contextmenu = document.getAnonymousElementByAttribute(textbox, "anonid", "input-box-contextmenu");
  let contextMenuShown = promisePanelElementShown(window, contextmenu);
  EventUtils.synthesizeMouseAtCenter(searchbar, {type: "contextmenu", button: 2});
  await contextMenuShown;

  ok(isOverflowOpen(), "Panel should still be open");

  let selectAll = contextmenu.querySelector("[cmd='cmd_selectAll']");
  let contextMenuHidden = promisePanelElementHidden(window, contextmenu);
  EventUtils.synthesizeMouseAtCenter(selectAll, {});
  await contextMenuHidden;

  // Hide the suggestion panel.
  searchbar.textbox.popup.hidePopup();

  ok(isOverflowOpen(), "Panel should still be open");

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await hiddenPanelPromise;
  ok(!isOverflowOpen(), "Panel should no longer be open");

  // We focused the search bar earlier - ensure we don't keep doing that.
  gURLBar.select();

  CustomizableUI.reset();
});

add_task(async function disabled_button_in_panel() {
  button = document.createElement("toolbarbutton");
  button.id = "browser_946166_button_disabled";
  button.setAttribute("disabled", "true");
  button.setAttribute("label", "Button");
  gNavToolbox.palette.appendChild(button);
  CustomizableUI.addWidgetToArea(button.id, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  EventUtils.synthesizeMouseAtCenter(button, {});
  is(PanelUI.overflowPanel.state, "open", "Popup stays open");
  button.removeAttribute("disabled");
  let hiddenAgain = promiseOverflowHidden(window);
  EventUtils.synthesizeMouseAtCenter(button, {});
  await hiddenAgain;
  button.remove();
});

registerCleanupFunction(function() {
  if (button && button.parentNode) {
    button.remove();
  }
  if (menuButton && menuButton.parentNode) {
    menuButton.remove();
  }
  // Sadly this isn't task.jsm-enabled, so we can't wait for this to happen. But we should
  // definitely close it here and hope it won't interfere with other tests.
  // Of course, all the tests are meant to do this themselves, but if they fail...
  if (isOverflowOpen()) {
    PanelUI.overflowPanel.hidePopup();
  }
  CustomizableUI.reset();
});
