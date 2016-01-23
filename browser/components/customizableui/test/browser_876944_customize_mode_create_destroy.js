/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestWidget1 = "test-customize-mode-create-destroy1";
const kTestWidget2 = "test-customize-mode-create-destroy2";

// Creating and destroying a widget should correctly wrap/unwrap stuff
add_task(function* testWrapUnwrap() {
  yield startCustomizing();
  CustomizableUI.createWidget({id: kTestWidget1, label: 'Pretty label', tooltiptext: 'Pretty tooltip'});
  let elem = document.getElementById(kTestWidget1);
  let wrapper = document.getElementById("wrapper-" + kTestWidget1);
  ok(elem, "There should be an item");
  ok(wrapper, "There should be a wrapper");
  is(wrapper.firstChild.id, kTestWidget1, "Wrapper should have test widget");
  is(wrapper.parentNode.id, "customization-palette", "Wrapper should be in palette");
  CustomizableUI.destroyWidget(kTestWidget1);
  wrapper = document.getElementById("wrapper-" + kTestWidget1);
  ok(!wrapper, "There should be a wrapper");
  let item = document.getElementById(kTestWidget1);
  ok(!item, "There should no longer be an item");
});

// Creating and destroying a widget should correctly deal with panel placeholders
add_task(function* testPanelPlaceholders() {
  let panel = document.getElementById(CustomizableUI.AREA_PANEL);
  // The value of expectedPlaceholders depends on the default palette layout.
  // Bug 1229236 is for these tests to be smarter so the test doesn't need to
  // change when the default placements change.
  let expectedPlaceholders = 1 + (isInDevEdition() ? 1 : 0);
  is(panel.querySelectorAll(".panel-customization-placeholder").length, expectedPlaceholders, "The number of placeholders should be correct.");
  CustomizableUI.createWidget({id: kTestWidget2, label: 'Pretty label', tooltiptext: 'Pretty tooltip', defaultArea: CustomizableUI.AREA_PANEL});
  let elem = document.getElementById(kTestWidget2);
  let wrapper = document.getElementById("wrapper-" + kTestWidget2);
  ok(elem, "There should be an item");
  ok(wrapper, "There should be a wrapper");
  is(wrapper.firstChild.id, kTestWidget2, "Wrapper should have test widget");
  is(wrapper.parentNode, panel, "Wrapper should be in panel");
  expectedPlaceholders = isInDevEdition() ? 1 : 3;
  is(panel.querySelectorAll(".panel-customization-placeholder").length, expectedPlaceholders, "The number of placeholders should be correct.");
  CustomizableUI.destroyWidget(kTestWidget2);
  wrapper = document.getElementById("wrapper-" + kTestWidget2);
  ok(!wrapper, "There should be a wrapper");
  let item = document.getElementById(kTestWidget2);
  ok(!item, "There should no longer be an item");
  yield endCustomizing();
});

add_task(function* asyncCleanup() {
  yield endCustomizing();
  try {
    CustomizableUI.destroyWidget(kTestWidget1);
  } catch (ex) {}
  try {
    CustomizableUI.destroyWidget(kTestWidget2);
  } catch (ex) {}
  yield resetCustomization();
});
