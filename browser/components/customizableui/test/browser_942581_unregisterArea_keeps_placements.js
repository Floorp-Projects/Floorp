/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-unregisterArea-placements-toolbar";
const kTestWidgetPfx = "test-widget-for-unregisterArea-placements-";
const kTestWidgetCount = 3;
registerCleanupFunction(removeCustomToolbars);

// unregisterArea should keep placements by default and restore them when re-adding the area
add_task(function() {
  let widgetIds = [];
  for (let i = 0; i < kTestWidgetCount; i++) {
    let id = kTestWidgetPfx + i;
    widgetIds.push(id);
    let spec = {id: id, type: 'button', removable: true, label: "unregisterArea test", tooltiptext: "" + i};
    CustomizableUI.createWidget(spec);
  }
  for (let i = kTestWidgetCount; i < kTestWidgetCount * 2; i++) {
    let id = kTestWidgetPfx + i;
    widgetIds.push(id);
    createDummyXULButton(id, "unregisterArea XUL test " + i);
  }
  let toolbarNode = createToolbarWithPlacements(kToolbarName, widgetIds);
  checkAbstractAndRealPlacements(toolbarNode, widgetIds);

  // Now move one of them:
  CustomizableUI.moveWidgetWithinArea(kTestWidgetPfx + kTestWidgetCount, 0);
  // Clone the array so we know this is the modified one:
  let modifiedWidgetIds = [...widgetIds];
  let movedWidget = modifiedWidgetIds.splice(kTestWidgetCount, 1)[0];
  modifiedWidgetIds.unshift(movedWidget);

  // Check it:
  checkAbstractAndRealPlacements(toolbarNode, modifiedWidgetIds);

  // Then unregister
  CustomizableUI.unregisterArea(kToolbarName);

  // Check we tell the outside world no dangerous things:
  checkWidgetFates(widgetIds);
  // Only then remove the real node
  toolbarNode.remove();

  // Now move one of the items to the palette, and another to the navbar:
  let lastWidget = modifiedWidgetIds.pop();
  CustomizableUI.removeWidgetFromArea(lastWidget);
  lastWidget = modifiedWidgetIds.pop();
  CustomizableUI.addWidgetToArea(lastWidget, CustomizableUI.AREA_NAVBAR);

  // Recreate ourselves with the default placements being the same:
  toolbarNode = createToolbarWithPlacements(kToolbarName, widgetIds);
  // Then check that after doing this, our actual placements match
  // the modified list, not the default one.
  checkAbstractAndRealPlacements(toolbarNode, modifiedWidgetIds);

  // Now remove completely:
  CustomizableUI.unregisterArea(kToolbarName, true);
  checkWidgetFates(modifiedWidgetIds);
  toolbarNode.remove();

  // One more time:
  // Recreate ourselves with the default placements being the same:
  toolbarNode = createToolbarWithPlacements(kToolbarName, widgetIds);
  // Should now be back to default:
  checkAbstractAndRealPlacements(toolbarNode, widgetIds);
  CustomizableUI.unregisterArea(kToolbarName, true);
  checkWidgetFates(widgetIds);
  toolbarNode.remove();

  //XXXgijs: ensure cleanup function doesn't barf:
  gAddedToolbars.delete(kToolbarName);

  // Remove all the XUL widgets, destroy the others:
  for (let widget of widgetIds) {
    let widgetWrapper = CustomizableUI.getWidget(widget);
    if (widgetWrapper.provider == CustomizableUI.PROVIDER_XUL) {
      gNavToolbox.palette.querySelector("#" + widget).remove();
    } else {
      CustomizableUI.destroyWidget(widget);
    }
  }
});

function checkAbstractAndRealPlacements(aNode, aExpectedPlacements) {
  assertAreaPlacements(kToolbarName, aExpectedPlacements);
  let physicalWidgetIds = [node.id for (node of aNode.childNodes)];
  placementArraysEqual(aNode.id, physicalWidgetIds, aExpectedPlacements);
}

function checkWidgetFates(aWidgetIds) {
  for (let widget of aWidgetIds) {
    ok(!CustomizableUI.getPlacementOfWidget(widget), "Widget should be in palette");
    ok(!document.getElementById(widget), "Widget should not be in the DOM");
    let widgetInPalette = !!gNavToolbox.palette.querySelector("#" + widget);
    let widgetProvider = CustomizableUI.getWidget(widget).provider;
    let widgetIsXULWidget = widgetProvider == CustomizableUI.PROVIDER_XUL;
    is(widgetInPalette, widgetIsXULWidget, "Just XUL Widgets should be in the palette");
  }
}

add_task(function asyncCleanup() {
  yield resetCustomization();
});
