/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
let gTests = [
  {
    desc: "Resize to a small window, resize back, shouldn't affect currentSet",
    run: function() {
      let originalWindowWidth = window.outerWidth;
      let oldCurrentSet = navbar.currentSet;
      ok(!navbar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
      ok(CustomizableUI.inDefaultState, "Should start in default state.");
      window.resizeTo(400, window.outerHeight);
      yield waitForCondition(() => navbar.hasAttribute("overflowing"));
      ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
      is(navbar.currentSet, oldCurrentSet, "Currentset should be the same when overflowing.");
      ok(CustomizableUI.inDefaultState, "Should still be in default state when overflowing.");
      window.resizeTo(originalWindowWidth, window.outerHeight);
      yield waitForCondition(() => !navbar.hasAttribute("overflowing"));
      ok(!navbar.hasAttribute("overflowing"), "Should no longer have an overflowing toolbar.");
      is(navbar.currentSet, oldCurrentSet, "Currentset should still be the same now we're no longer overflowing.");
      ok(CustomizableUI.inDefaultState, "Should still be in default state now we're no longer overflowing.");
    }
  },
  {
    desc: "Enter and exit customization mode, check that currentSet works",
    run: function() {
      let oldCurrentSet = navbar.currentSet;
      ok(CustomizableUI.inDefaultState, "Should start in default state.");
      yield startCustomizing();
      ok(CustomizableUI.inDefaultState, "Should be in default state in customization mode.");
      is(navbar.currentSet, oldCurrentSet, "Currentset should be the same in customization mode.");
      yield endCustomizing();
      ok(CustomizableUI.inDefaultState, "Should be in default state after customization mode.");
      is(navbar.currentSet, oldCurrentSet, "Currentset should be the same after customization mode.");
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
