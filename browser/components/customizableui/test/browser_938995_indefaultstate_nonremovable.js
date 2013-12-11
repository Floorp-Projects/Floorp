/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "test-non-removable-widget";
let gTests = [
  {
    desc: "Adding non-removable items to a toolbar or the panel shouldn't change inDefaultState",
    run: function() {
      let navbar = document.getElementById("nav-bar");
      ok(CustomizableUI.inDefaultState, "Should start in default state");

      CustomizableUI.createWidget({id: kWidgetId, removable: false, label: "Test"});
      CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);
      ok(CustomizableUI.inDefaultState, "Should still be in default state after navbar addition");
      CustomizableUI.destroyWidget(kWidgetId);

      CustomizableUI.createWidget({id: kWidgetId, removable: false, label: "Test"});
      CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_PANEL);
      ok(CustomizableUI.inDefaultState, "Should still be in default state after panel addition");
      CustomizableUI.destroyWidget(kWidgetId);

      ok(CustomizableUI.inDefaultState, "Should be in default state after destroying both widgets");
    },
    teardown: null
  },
];

function test() {
  waitForExplicitFinish();
  runTests(gTests);
}

