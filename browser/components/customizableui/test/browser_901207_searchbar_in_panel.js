/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ctrl+K should open the menu panel and focus the search bar if the search bar is in the panel.
add_task(function() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");

  let shownPanelPromise = promisePanelShown(window);
  sendWebSearchKeyCommand();
  yield shownPanelPromise;

  logActiveElement();
  is(document.activeElement, searchbar.textbox.inputField, "The searchbar should be focused");

  let hiddenPanelPromise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should give focus to the searchbar when the searchbar is in the menupanel and the panel is already opened.
add_task(function() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");

  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPanelPromise;

  sendWebSearchKeyCommand();
  logActiveElement();
  is(document.activeElement, searchbar.textbox.inputField, "The searchbar should be focused");

  let hiddenPanelPromise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should open the overflow panel and focus the search bar if the search bar is overflowed.
add_task(function() {
  this.originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(360, window.outerHeight);
  yield waitForCondition(() => navbar.getAttribute("overflowing") == "true");
  ok(!navbar.querySelector("#search-container"), "Search container should be overflowing");
  let searchbar = document.getElementById("searchbar");

  let shownPanelPromise = promiseOverflowShown(window);
  sendWebSearchKeyCommand();
  yield shownPanelPromise;

  let chevron = document.getElementById("nav-bar-overflow-button");
  yield waitForCondition(function() chevron.open);
  logActiveElement();
  is(document.activeElement, searchbar.textbox.inputField, "The searchbar should be focused");

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield hiddenPanelPromise;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  window.resizeTo(this.originalWindowWidth, window.outerHeight);
  yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
});

// Ctrl+K should focus the search bar if it is in the navbar and not overflowing.
add_task(function() {
  let searchbar = document.getElementById("searchbar");
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Should be in nav-bar");

  sendWebSearchKeyCommand();
  logActiveElement();
  is(document.activeElement, searchbar.textbox.inputField, "The searchbar should be focused");
});

// Ctrl+K should open the search page if the search bar has been customized out.
add_task(function() {
  this.originalOpenUILinkIn = openUILinkIn;
  try {
    CustomizableUI.removeWidgetFromArea("search-container");
    let placement = CustomizableUI.getPlacementOfWidget("search-container");
    is(placement, null, "Search container should be in palette");

    let openUILinkInCalled = false;
    openUILinkIn = (aUrl, aWhichTab) => {
      is(aUrl, Services.search.defaultEngine.searchForm, "Search page should be requested to open.");
      is(aWhichTab, "current", "Should use the current tab for the search page.");
      openUILinkInCalled = true;
    };
    sendWebSearchKeyCommand();
    yield waitForCondition(function() openUILinkInCalled);
    ok(openUILinkInCalled, "The search page should have been opened.")
  } catch (e) {
    ok(false, e);
  }
  openUILinkIn = this.originalOpenUILinkIn;
  CustomizableUI.reset();
});

function sendWebSearchKeyCommand() {
  if (Services.appinfo.OS === "Darwin")
    EventUtils.synthesizeKey("k", { accelKey: true });
  else
    EventUtils.synthesizeKey("k", { ctrlKey: true });
}

function logActiveElement() {
  let element = document.activeElement;
  info("Active element: " + element ?
    element + " (" + element.localName + "#" + element.id + "." + [...element.classList].join(".") + ")" :
    "null");
}
