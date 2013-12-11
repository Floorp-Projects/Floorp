/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kTestBtnId = "test-removable-navbar-customize-mode";

let gTests = [
  {
    desc: "Items without the removable attribute in the navbar should be considered non-removable",
    setup: function() {
      let btn = createDummyXULButton(kTestBtnId, "Test removable in navbar in customize mode");
      document.getElementById("nav-bar").customizationTarget.appendChild(btn);
      return startCustomizing();
    },
    run: function() {
      ok(!CustomizableUI.isWidgetRemovable(kTestBtnId), "Widget should not be considered removable");
    },
    teardown: function() {
      yield endCustomizing();
      document.getElementById(kTestBtnId).remove();
    }
  },
];
function asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
}

function test() {
  waitForExplicitFinish();
  runTests(gTests, asyncCleanup);
}

