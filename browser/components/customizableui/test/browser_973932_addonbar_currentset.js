/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var addonbarID = CustomizableUI.AREA_ADDONBAR;
var addonbar = document.getElementById(addonbarID);

// Check that currentset is correctly updated after a reset:
add_task(function*() {
  let placements = CustomizableUI.getWidgetIdsInArea(addonbarID);
  is(placements.join(','), addonbar.getAttribute("currentset"), "Addon-bar currentset should match default placements");
  ok(CustomizableUI.inDefaultState, "Should be in default state");
  info("Adding a spring to add-on bar shim");
  CustomizableUI.addWidgetToArea("spring", addonbarID, 1);
  ok(addonbar.getElementsByTagName("toolbarspring").length, "There should be a spring in the toolbar");
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state");
  placements = CustomizableUI.getWidgetIdsInArea(addonbarID);
  is(placements.join(','), addonbar.getAttribute("currentset"), "Addon-bar currentset should match placements after spring addition");

  yield startCustomizing();
  yield gCustomizeMode.reset();
  ok(CustomizableUI.inDefaultState, "Should be in default state after reset");
  placements = CustomizableUI.getWidgetIdsInArea(addonbarID);
  is(placements.join(','), addonbar.getAttribute("currentset"), "Addon-bar currentset should match default placements after reset");
  ok(!addonbar.getElementsByTagName("toolbarspring").length, "There should be no spring in the toolbar");
  yield endCustomizing();
});

