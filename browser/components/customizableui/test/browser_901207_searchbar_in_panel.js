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
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");

  let shownPanelPromise = promisePanelShown(window);
  sendWebSearchKeyCommand();
  await shownPanelPromise;

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should give focus to the searchbar when the searchbar is in the menupanel and the panel is already opened.
add_task(async function() {
  let searchbar = document.getElementById("searchbar");
  gCustomizeMode.addToPanel(searchbar);
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_PANEL, "Should be in panel");

  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  await shownPanelPromise;

  sendWebSearchKeyCommand();

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await hiddenPanelPromise;
  CustomizableUI.reset();
});

// Ctrl+K should open the overflow panel and focus the search bar if the search bar is overflowed.
add_task(async function() {
  this.originalWindowWidth = window.outerWidth;
  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  window.resizeTo(360, window.outerHeight);
  await waitForCondition(() => navbar.getAttribute("overflowing") == "true");
  ok(!navbar.querySelector("#search-container"), "Search container should be overflowing");

  let shownPanelPromise = promiseOverflowShown(window);
  sendWebSearchKeyCommand();
  await shownPanelPromise;

  let chevron = document.getElementById("nav-bar-overflow-button");
  await waitForCondition(() => chevron.open);

  await waitForSearchBarFocus();

  let hiddenPanelPromise = promiseOverflowHidden(window);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await hiddenPanelPromise;
  navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  window.resizeTo(this.originalWindowWidth, window.outerHeight);
  await waitForCondition(() => !navbar.hasAttribute("overflowing"));
  ok(!navbar.hasAttribute("overflowing"), "Should not have an overflowing toolbar.");
});

// Ctrl+K should focus the search bar if it is in the navbar and not overflowing.
add_task(async function() {
  let placement = CustomizableUI.getPlacementOfWidget("search-container");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Should be in nav-bar");

  sendWebSearchKeyCommand();

  await waitForSearchBarFocus();
});


function sendWebSearchKeyCommand() {
  if (Services.appinfo.OS === "Darwin")
    EventUtils.synthesizeKey("k", { accelKey: true });
  else
    EventUtils.synthesizeKey("k", { ctrlKey: true });
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
