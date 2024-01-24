/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-new-overflowable-toolbar";
const kTestWidgetPrefix = "test-widget-for-overflowable-toolbar-";

add_task(async function addOverflowingToolbar() {
  let originalWindowWidth = window.outerWidth;

  let widgetIds = [];
  registerCleanupFunction(() => {
    try {
      for (let id of widgetIds) {
        CustomizableUI.destroyWidget(id);
      }
    } catch (ex) {
      console.error(ex);
    }
  });

  for (let i = 0; i < 10; i++) {
    let id = kTestWidgetPrefix + i;
    widgetIds.push(id);
    let spec = {
      id,
      type: "button",
      removable: true,
      label: "test",
      tooltiptext: "" + i,
    };
    CustomizableUI.createWidget(spec);
  }

  let toolbarNode = createOverflowableToolbarWithPlacements(
    kToolbarName,
    widgetIds
  );
  assertAreaPlacements(kToolbarName, widgetIds);

  for (let id of widgetIds) {
    document.getElementById(id).style.minWidth = "200px";
  }

  isnot(
    toolbarNode.overflowable,
    null,
    "Toolbar should have overflowable controller"
  );
  isnot(
    CustomizableUI.getCustomizationTarget(toolbarNode),
    null,
    "Toolbar should have customization target"
  );
  isnot(
    CustomizableUI.getCustomizationTarget(toolbarNode),
    toolbarNode,
    "Customization target should not be toolbar node"
  );

  let oldChildCount =
    CustomizableUI.getCustomizationTarget(toolbarNode).childElementCount;
  let overflowableList = document.getElementById(
    kToolbarName + "-overflow-list"
  );
  let oldOverflowCount = overflowableList.childElementCount;

  isnot(oldChildCount, 0, "Toolbar should have non-overflowing widgets");

  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() =>
    toolbarNode.hasAttribute("overflowing")
  );
  ok(
    toolbarNode.hasAttribute("overflowing"),
    "Should have an overflowing toolbar."
  );
  Assert.less(
    CustomizableUI.getCustomizationTarget(toolbarNode).childElementCount,
    oldChildCount,
    "Should have fewer children."
  );
  Assert.greater(
    overflowableList.childElementCount,
    oldOverflowCount,
    "Should have more overflowed widgets."
  );

  window.resizeTo(originalWindowWidth, window.outerHeight);
});

add_task(async function asyncCleanup() {
  removeCustomToolbars();
  await resetCustomization();
});
