/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarId = "test-registerToolbarNode-toolbar";
const kButtonId = "test-registerToolbarNode-button";
registerCleanupFunction(cleanup);

// Registering a toolbar without a defaultset attribute should
// wait for the registerArea call
add_task(async function() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
  let btn = createDummyXULButton(kButtonId);
  let toolbar = document.createXULElement("toolbar");
  toolbar.id = kToolbarId;
  toolbar.setAttribute("customizable", true);
  gNavToolbox.appendChild(toolbar);
  CustomizableUI.registerToolbarNode(toolbar);
  ok(!CustomizableUI.areas.includes(kToolbarId),
     "Toolbar should not yet have been registered automatically.");
  CustomizableUI.registerArea(kToolbarId, {defaultPlacements: [kButtonId]});
  ok(CustomizableUI.areas.includes(kToolbarId),
     "Toolbar should have been registered now.");
  is(CustomizableUI.getAreaType(kToolbarId), CustomizableUI.TYPE_TOOLBAR,
     "Area should be registered as toolbar");
  assertAreaPlacements(kToolbarId, [kButtonId]);
  ok(!CustomizableUI.inDefaultState, "No longer in default state after toolbar is registered and visible.");
  CustomizableUI.unregisterArea(kToolbarId, true);
  toolbar.remove();
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
  btn.remove();
});

async function cleanup() {
  let toolbar = document.getElementById(kToolbarId);
  if (toolbar) {
    toolbar.remove();
  }
  let btn = document.getElementById(kButtonId) ||
            gNavToolbox.querySelector("#" + kButtonId);
  if (btn) {
    btn.remove();
  }
  await resetCustomization();
}
