/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests = [
  {
    run: function() {
      const kButtonId = "look-at-me-disappear-button";
      CustomizableUI.reset();
      ok(CustomizableUI.inDefaultState, "Should be in the default state.");
      let btn = createDummyXULButton(kButtonId, "Gone!");
      CustomizableUI.addWidgetToArea(kButtonId, CustomizableUI.AREA_NAVBAR);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in the default state.");
      is(btn.parentNode.parentNode.id, CustomizableUI.AREA_NAVBAR, "Button should be in navbar");
      btn.remove();
      is(btn.parentNode, null, "Button is no longer in the navbar");
      ok(CustomizableUI.inDefaultState, "Should be back in the default state, " +
                                        "despite unknown button ID in placements.");
    }
  }
];

function asyncCleanup() {
  yield resetCustomization();
}

function cleanup() {
  removeCustomToolbars();
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanup);
  runTests(gTests, asyncCleanup);
}
