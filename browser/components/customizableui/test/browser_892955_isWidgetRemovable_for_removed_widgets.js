/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "test-892955-remove-widget";

let gTests = [
  {
    desc: "Removing a destroyed widget should work.",
    run: function() {
      let widgetSpec = {
        id: kWidgetId,
        defaultArea: CustomizableUI.AREA_NAVBAR
      };

      CustomizableUI.createWidget(widgetSpec);
      CustomizableUI.destroyWidget(kWidgetId);
      let noError = true;
      try {
        CustomizableUI.removeWidgetFromArea(kWidgetId);
      } catch (ex) {
        noError = false;
        Cu.reportError(ex);
      }
      ok(noError, "Shouldn't throw an error removing a destroyed widget.");
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
