/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-new-overflowable-toolbar";
const kTestWidgetPrefix = "test-widget-for-overflowable-toolbar-";

add_task(function* addOverflowingToolbar() {
  let originalWindowWidth = window.outerWidth;

  let widgetIds = [];
  for (let i = 0; i < 10; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {id: id, type: "button", removable: true, label: "test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
  }

  let toolbarNode = createOverflowableToolbarWithPlacements(kToolbarName, widgetIds);
  assertAreaPlacements(kToolbarName, widgetIds);

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  isnot(toolbarNode.overflowable, null, "Toolbar should have overflowable controller");
  isnot(toolbarNode.customizationTarget, null, "Toolbar should have customization target");
  isnot(toolbarNode.customizationTarget, toolbarNode, "Customization target should not be toolbar node");

  let oldChildCount = toolbarNode.customizationTarget.childElementCount;
  let overflowableList = document.getElementById(kToolbarName + "-overflow-list");
  let oldOverflowCount = overflowableList.childElementCount;

  isnot(oldChildCount, 0, "Toolbar should have non-overflowing widgets");

  window.resizeTo(400, window.outerHeight);
  yield waitForCondition(() => toolbarNode.hasAttribute("overflowing"));
  ok(toolbarNode.hasAttribute("overflowing"), "Should have an overflowing toolbar.");
  ok(toolbarNode.customizationTarget.childElementCount < oldChildCount, "Should have fewer children.");
  ok(overflowableList.childElementCount > oldOverflowCount, "Should have more overflowed widgets.");

  window.resizeTo(originalWindowWidth, window.outerHeight);
});


add_task(function* asyncCleanup() {
  removeCustomToolbars();
  yield resetCustomization();
});
