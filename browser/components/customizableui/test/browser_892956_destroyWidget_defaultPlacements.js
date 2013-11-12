/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "test-892956-destroyWidget-defaultPlacement";

let gTests = [
  {
    desc: "destroyWidget should clean up defaultPlacements if the widget had a defaultArea",
    run: function() {
      ok(CustomizableUI.inDefaultState, "Should be in the default state when we start");

      let widgetSpec = {
        id: kWidgetId,
        defaultArea: CustomizableUI.AREA_NAVBAR
      };
      CustomizableUI.createWidget(widgetSpec);
      CustomizableUI.destroyWidget(kWidgetId);
      ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
    }
  }
];

function asyncCleanup() {
  yield resetCustomization();
}

function test() {
  waitForExplicitFinish();
  runTests(gTests, asyncCleanup);
}
