/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var button, menuButton;
/* Clicking a button should close the panel */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  button = document.createElement("toolbarbutton");
  button.id = "browser_940307_button";
  button.setAttribute("label", "Button");
  PanelUI.contents.appendChild(button);
  await PanelUI.show();
  let hiddenAgain = promisePanelHidden(window);
  EventUtils.synthesizeMouseAtCenter(button, {});
  await hiddenAgain;
  button.remove();
});

/* Clicking a menu button should close the panel, opening the popup shouldn't.  */
add_task(async function() {
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
  PanelUI.contents.appendChild(menuButton);

  await PanelUI.show();
  let hiddenAgain = promisePanelHidden(window);
  let innerButton = document.getAnonymousElementByAttribute(menuButton, "anonid", "button");
  EventUtils.synthesizeMouseAtCenter(innerButton, {});
  await hiddenAgain;

  // Now click the dropmarker to show the menu
  await PanelUI.show();
  hiddenAgain = promisePanelHidden(window);
  let menuShown = promisePanelElementShown(window, menuPopup);
  let dropmarker = document.getAnonymousElementByAttribute(menuButton, "type", "menu-button");
  EventUtils.synthesizeMouseAtCenter(dropmarker, {});
  await menuShown;
  // Panel should stay open:
  ok(isPanelUIOpen(), "Panel should still be open");
  let menuHidden = promisePanelElementHidden(window, menuPopup);
  // Then click the menu item to close all the things
  EventUtils.synthesizeMouseAtCenter(menuItem, {});
  await menuHidden;
  await hiddenAgain;
  menuButton.remove();
});

add_task(async function() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");
  await PanelUI.show();
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

  ok(isPanelUIOpen(), "Panel should still be open");

  let selectAll = contextmenu.querySelector("[cmd='cmd_selectAll']");
  let contextMenuHidden = promisePanelElementHidden(window, contextmenu);
  EventUtils.synthesizeMouseAtCenter(selectAll, {});
  await contextMenuHidden;

  // Hide the suggestion panel.
  searchbar.textbox.popup.hidePopup();

  ok(isPanelUIOpen(), "Panel should still be open");

  let hiddenPanelPromise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await hiddenPanelPromise;
  ok(!isPanelUIOpen(), "Panel should no longer be open");

  // We focused the search bar earlier - ensure we don't keep doing that.
  gURLBar.select();

  CustomizableUI.reset();
});

add_task(async function() {
  button = document.createElement("toolbarbutton");
  button.id = "browser_946166_button_disabled";
  button.setAttribute("disabled", "true");
  button.setAttribute("label", "Button");
  PanelUI.contents.appendChild(button);
  await PanelUI.show();
  EventUtils.synthesizeMouseAtCenter(button, {});
  is(PanelUI.panel.state, "open", "Popup stays open");
  button.removeAttribute("disabled");
  let hiddenAgain = promisePanelHidden(window);
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
  if (isPanelUIOpen()) {
    PanelUI.hide();
  }
});
