/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Restoring default should not place addon widgets back in the toolbar
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Default state to begin");

  const kWidgetId = "bug982656-add-on-widget-should-not-restore-to-default-area";
  let widgetSpec = {
    id: kWidgetId,
    defaultArea: CustomizableUI.AREA_NAVBAR
  };
  CustomizableUI.createWidget(widgetSpec);

  ok(!CustomizableUI.inDefaultState, "Not in default state after widget added");
  is(CustomizableUI.getPlacementOfWidget(kWidgetId).area, CustomizableUI.AREA_NAVBAR, "Widget should be in navbar");

  yield resetCustomization();

  ok(CustomizableUI.inDefaultState, "Back in default state after reset");
  is(CustomizableUI.getPlacementOfWidget(kWidgetId), null, "Widget now in palette");
  CustomizableUI.destroyWidget(kWidgetId);
});


// resetCustomization shouldn't move 3rd party widgets out of custom toolbars
add_task(function() {
  const kToolbarId = "bug982656-toolbar-with-defaultset";
  const kWidgetId = "bug982656-add-on-widget-should-restore-to-default-area-when-area-is-not-builtin";
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
  let toolbar = createToolbarWithPlacements(kToolbarId);
  ok(CustomizableUI.areas.indexOf(kToolbarId) != -1,
     "Toolbar has been registered.");
  is(CustomizableUI.getAreaType(kToolbarId), CustomizableUI.TYPE_TOOLBAR,
     "Area should be registered as toolbar");

  let widgetSpec = {
    id: kWidgetId,
    defaultArea: kToolbarId
  };
  CustomizableUI.createWidget(widgetSpec);

  ok(!CustomizableUI.inDefaultState, "No longer in default state after toolbar is registered and visible.");
  is(CustomizableUI.getPlacementOfWidget(kWidgetId).area, kToolbarId, "Widget should be in custom toolbar");

  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "Back in default state after reset");
  is(CustomizableUI.getPlacementOfWidget(kWidgetId).area, kToolbarId, "Widget still in custom toolbar");
  ok(toolbar.collapsed, "Custom toolbar should be collapsed after reset");

  toolbar.remove();
  CustomizableUI.destroyWidget(kWidgetId);
  CustomizableUI.unregisterArea(kToolbarId);
});
