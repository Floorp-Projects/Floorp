/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Attempting to drag the menubar to the navbar shouldn't work.
add_task(function() {
  yield startCustomizing();
  let menuItems = document.getElementById("menubar-items");
  let navbar = document.getElementById("nav-bar");
  let menubar = document.getElementById("toolbar-menubar");
  // Force the menu to be shown.
  const kAutohide = menubar.getAttribute("autohide");
  menubar.setAttribute("autohide", "false");
  simulateItemDrag(menuItems, navbar.customizationTarget);

  is(getAreaWidgetIds("nav-bar").indexOf("menubar-items"), -1, "Menu bar shouldn't be in the navbar.");
  ok(!navbar.querySelector("#menubar-items"), "Shouldn't find menubar items in the navbar.");
  ok(menubar.querySelector("#menubar-items"), "Should find menubar items in the menubar.");
  isnot(getAreaWidgetIds("toolbar-menubar").indexOf("menubar-items"), -1,
        "Menubar items shouldn't be missing from the navbar.");
  menubar.setAttribute("autohide", kAutohide);
  yield endCustomizing();
});

add_task(function asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});
