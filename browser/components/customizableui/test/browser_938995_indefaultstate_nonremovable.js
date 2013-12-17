/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kWidgetId = "test-non-removable-widget";

// Adding non-removable items to a toolbar or the panel shouldn't change inDefaultState
add_task(function() {
  let navbar = document.getElementById("nav-bar");
  ok(CustomizableUI.inDefaultState, "Should start in default state");

  let button = createDummyXULButton(kWidgetId, "Test non-removable inDefaultState handling");
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);
  button.setAttribute("removable", "false");
  ok(CustomizableUI.inDefaultState, "Should still be in default state after navbar addition");
  button.remove();

  button = createDummyXULButton(kWidgetId, "Test non-removable inDefaultState handling");
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_PANEL);
  button.setAttribute("removable", "false");
  ok(CustomizableUI.inDefaultState, "Should still be in default state after panel addition");
  button.remove();
  ok(CustomizableUI.inDefaultState, "Should be in default state after destroying both widgets");
});
