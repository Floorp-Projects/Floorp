/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "test-destroy-in-palette";

let gTests = [
  {
    desc: "Check destroyWidget destroys the node if it's in the palette",
    run: function() {
      CustomizableUI.createWidget({id: kWidgetId, label: "Test destroying widgets in palette."});
      yield startCustomizing();
      yield endCustomizing();
      ok(gNavToolbox.palette.querySelector("#" + kWidgetId), "Widget still exists in palette.");
      CustomizableUI.destroyWidget(kWidgetId);
      ok(!gNavToolbox.palette.querySelector("#" + kWidgetId), "Widget no longer exists in palette.");
    },
  },
];

function test() {
  waitForExplicitFinish();
  runTests(gTests);
}

