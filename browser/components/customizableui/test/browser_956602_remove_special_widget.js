/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


// Adding a separator and then dragging it out of the navbar shouldn't throw
add_task(function() {
  try {
    let navbar = document.getElementById("nav-bar");
    let separatorSelector = "toolbarseparator[id^=customizableui-special-separator]";
    ok(!navbar.querySelector(separatorSelector), "Shouldn't be a separator in the navbar");
    CustomizableUI.addWidgetToArea('separator', 'nav-bar');
    yield startCustomizing();
    let separator = navbar.querySelector(separatorSelector);
    ok(separator, "There should be a separator in the navbar now.");
    let palette = document.getElementById("customization-palette");
    simulateItemDrag(separator, palette);
    ok(!palette.querySelector(separatorSelector), "No separator in the palette.");
  } catch (ex) {
    Cu.reportError(ex);
    ok(false, "Shouldn't throw an exception moving an item to the navbar.");
  } finally {
    yield endCustomizing();
  }
});

add_task(function asyncCleanup() {
  resetCustomization();
});
