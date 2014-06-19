/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Adding, moving and removing items should update the relevant currentset attributes
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Should be in the default state when we start");
  let personalbar = document.getElementById(CustomizableUI.AREA_BOOKMARKS);
  setToolbarVisibility(personalbar, true);
  ok(!CustomizableUI.inDefaultState, "Making the bookmarks toolbar visible takes it out of the default state");

  let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  let personalbar = document.getElementById(CustomizableUI.AREA_BOOKMARKS);
  let navbarCurrentset = navbar.getAttribute("currentset") || navbar.currentSet;
  let personalbarCurrentset = personalbar.getAttribute("currentset") || personalbar.currentSet;

  let otherWin = yield openAndLoadWindow();
  let otherNavbar = otherWin.document.getElementById(CustomizableUI.AREA_NAVBAR);
  let otherPersonalbar = otherWin.document.getElementById(CustomizableUI.AREA_BOOKMARKS);

  CustomizableUI.moveWidgetWithinArea("home-button", 0);
  navbarCurrentset = "home-button," + navbarCurrentset.replace(",home-button", "");
  is(navbar.getAttribute("currentset"), navbarCurrentset,
     "Should have updated currentSet after move.");
  is(otherNavbar.getAttribute("currentset"), navbarCurrentset,
     "Should have updated other window's currentSet after move.");

  CustomizableUI.addWidgetToArea("home-button", CustomizableUI.AREA_BOOKMARKS);
  navbarCurrentset = navbarCurrentset.replace("home-button,", "");
  personalbarCurrentset = personalbarCurrentset + ",home-button";
  is(navbar.getAttribute("currentset"), navbarCurrentset,
     "Should have updated navbar currentSet after implied remove.");
  is(otherNavbar.getAttribute("currentset"), navbarCurrentset,
     "Should have updated other window's navbar currentSet after implied remove.");
  is(personalbar.getAttribute("currentset"), personalbarCurrentset,
     "Should have updated personalbar currentSet after add.");
  is(otherPersonalbar.getAttribute("currentset"), personalbarCurrentset,
     "Should have updated other window's personalbar currentSet after add.");

  CustomizableUI.removeWidgetFromArea("home-button");
  personalbarCurrentset = personalbarCurrentset.replace(",home-button", "");
  is(personalbar.getAttribute("currentset"), personalbarCurrentset,
     "Should have updated currentSet after remove.");
  is(otherPersonalbar.getAttribute("currentset"), personalbarCurrentset,
     "Should have updated other window's currentSet after remove.");

  yield promiseWindowClosed(otherWin);
  // Reset in asyncCleanup will put our button back for us.
});

add_task(function asyncCleanup() {
  let personalbar = document.getElementById(CustomizableUI.AREA_BOOKMARKS);
  setToolbarVisibility(personalbar, false);
  yield resetCustomization();
});
