/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Customize mode reset button should revert correctly
add_task(function*() {
  yield startCustomizing();
  let devButton = document.getElementById("developer-button");
  let downloadsButton = document.getElementById("downloads-button");
  let searchBox = document.getElementById("search-container");
  let palette = document.getElementById("customization-palette");
  ok(devButton && downloadsButton && searchBox && palette, "Stuff should exist");
  simulateItemDrag(devButton, downloadsButton);
  simulateItemDrag(searchBox, palette);
  yield gCustomizeMode.reset();
  ok(CustomizableUI.inDefaultState, "Should be back in default state");
  yield endCustomizing();
});

add_task(function* asyncCleanup() {
  yield resetCustomization();
});
