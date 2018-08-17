/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestWidget1 = "test-customize-mode-create-destroy1";

// Creating and destroying a widget should correctly wrap/unwrap stuff
add_task(async function testWrapUnwrap() {
  await startCustomizing();
  CustomizableUI.createWidget({id: kTestWidget1, label: "Pretty label", tooltiptext: "Pretty tooltip"});
  let elem = document.getElementById(kTestWidget1);
  let wrapper = document.getElementById("wrapper-" + kTestWidget1);
  ok(elem, "There should be an item");
  ok(wrapper, "There should be a wrapper");
  is(wrapper.firstElementChild.id, kTestWidget1, "Wrapper should have test widget");
  is(wrapper.parentNode.id, "customization-palette", "Wrapper should be in palette");
  CustomizableUI.destroyWidget(kTestWidget1);
  wrapper = document.getElementById("wrapper-" + kTestWidget1);
  ok(!wrapper, "There should be a wrapper");
  let item = document.getElementById(kTestWidget1);
  ok(!item, "There should no longer be an item");
});

add_task(async function asyncCleanup() {
  await endCustomizing();
  try {
    CustomizableUI.destroyWidget(kTestWidget1);
  } catch (ex) {}
  await resetCustomization();
});
