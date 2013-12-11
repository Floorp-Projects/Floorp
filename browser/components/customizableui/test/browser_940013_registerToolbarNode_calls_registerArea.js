/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kToolbarId = "test-registerToolbarNode-toolbar";
const kButtonId = "test-registerToolbarNode-button";
let gTests = [
  {
    desc: "Registering a toolbar with defaultset attribute should work",
    run: function() {
      let btn = createDummyXULButton(kButtonId);
      let toolbar = document.createElement("toolbar");
      toolbar.id = kToolbarId;
      toolbar.setAttribute("customizable", true);
      toolbar.setAttribute("defaultset", kButtonId);
      gNavToolbox.appendChild(toolbar);
      ok(CustomizableUI.areas.indexOf(kToolbarId) != -1,
         "Toolbar should have been registered automatically.");
      is(CustomizableUI.getAreaType(kToolbarId), CustomizableUI.TYPE_TOOLBAR,
         "Area should be registered as toolbar");
      assertAreaPlacements(kToolbarId, [kButtonId]);
      ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
      CustomizableUI.unregisterArea(kToolbarId, true);
      toolbar.remove();
      ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
      btn.remove();
    }
  },
  {
    desc: "Registering a toolbar without a defaultset attribute should " +
          "wait for the registerArea call",
    run: function() {
      let btn = createDummyXULButton(kButtonId);
      let toolbar = document.createElement("toolbar");
      toolbar.id = kToolbarId;
      toolbar.setAttribute("customizable", true);
      gNavToolbox.appendChild(toolbar);
      ok(CustomizableUI.areas.indexOf(kToolbarId) == -1,
         "Toolbar should not yet have been registered automatically.");
      CustomizableUI.registerArea(kToolbarId, {defaultPlacements: [kButtonId]});
      ok(CustomizableUI.areas.indexOf(kToolbarId) != -1,
         "Toolbar should have been registered now.");
      is(CustomizableUI.getAreaType(kToolbarId), CustomizableUI.TYPE_TOOLBAR,
         "Area should be registered as toolbar");
      assertAreaPlacements(kToolbarId, [kButtonId]);
      ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
      CustomizableUI.unregisterArea(kToolbarId, true);
      toolbar.remove();
      ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
      btn.remove();
    }
  }
];

function asyncCleanup() {
  yield resetCustomization();
}

function cleanup() {
  let toolbar = document.getElementById(kToolbarId);
  if (toolbar) {
    toolbar.remove();
  }
  let btn = document.getElementById(kButtonId) ||
            gNavToolbox.querySelector("#" + kButtonId);
  if (btn) {
    btn.remove();
  }
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanup);
  runTests(gTests, asyncCleanup);
}


