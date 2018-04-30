/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

logActiveElement();

async function waitForSearchBarFocus() {
  let searchbar = document.getElementById("searchbar");
  await waitForCondition(function() {
    logActiveElement();
    return document.activeElement === searchbar.textbox.inputField;
  });
}

// Ctrl+K should open the menu panel and focus the search bar if the search bar is in the panel.
add_task(async function() {
  CustomizableUI.addWidgetToArea("search-container",
                                 CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  let shownPanelPromise = promiseOverflowShown(window);
  sendWebSearchKeyCommand();
  await shownPanelPromise;

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("KEY_Escape");
  await hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should give focus to the searchbar when the searchbar is in the menupanel and the panel is already opened.
add_task(async function() {
  CustomizableUI.addWidgetToArea("search-container",
                                 CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  await document.getElementById("nav-bar").overflowable.show();

  sendWebSearchKeyCommand();

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("KEY_Escape");
  await hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should open the overflow panel and focus the search bar if the search bar is overflowed.
add_task(async function check_shortcut_when_in_overflow() {
  this.originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  Services.prefs.setBoolPref("browser.search.widget.inNavBar", true);

  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await waitForCondition(() => {
    return navbar.getAttribute("overflowing") == "true" &&
      !navbar.querySelector("#search-container");
  });
  ok(!navbar.querySelector("#search-container"), "Search container should be overflowing");

  let shownPanelPromise = promiseOverflowShown(window);
  sendWebSearchKeyCommand();
  await shownPanelPromise;

  let chevron = document.getElementById("nav-bar-overflow-button");
  await waitForCondition(() => chevron.open);

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("KEY_Escape");
  await hiddenPanelPromise;

  Services.prefs.setBoolPref("browser.search.widget.inNavBar", false);

  navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  window.resizeTo(this.originalWindowWidth, window.outerHeight);
  await waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
});

// Ctrl+K should focus the search bar if it is in the navbar and not overflowing.
add_task(async function() {
  Services.prefs.setBoolPref("browser.search.widget.inNavBar", true);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Should be in nav-bar");

  sendWebSearchKeyCommand();

  await waitForSearchBarFocus();

  Services.prefs.setBoolPref("browser.search.widget.inNavBar", false);
});


function sendWebSearchKeyCommand() {
  document.documentElement.focus();
  EventUtils.synthesizeKey("k", { accelKey: true });
}

function logActiveElement() {
  let element = document.activeElement;
  let str = "";
  while (element && element.parentNode) {
    str = " (" + element.localName + "#" + element.id + "." + [...element.classList].join(".") + ") >" + str;
    element = element.parentNode;
  }
  info("Active element: " + element ? str : "null");
}
