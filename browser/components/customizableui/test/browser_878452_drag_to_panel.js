/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
CustomizableUI.createWidget({id: "cui-panel-item-to-drag-to", defaultArea: CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, label: "Item in panel to drag to"});

// Dragging an item from the palette to another button in the panel should work.
add_task(async function() {
  await startCustomizing();
  let btn = document.getElementById("feed-button");
  let placements = getAreaWidgetIds(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  let lastButtonIndex = placements.length - 1;
  let lastButton = placements[lastButtonIndex];
  let placementsAfterInsert = placements.slice(0, lastButtonIndex).concat(["feed-button", lastButton]);
  let lastButtonNode = document.getElementById(lastButton);
  simulateItemDrag(btn, lastButtonNode, "start");
  assertAreaPlacements(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, placementsAfterInsert);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(btn, palette);
  CustomizableUI.removeWidgetFromArea("cui-panel-item-to-drag-to");
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// Dragging an item from the palette to the panel itself should also work.
add_task(async function() {
  CustomizableUI.addWidgetToArea("cui-panel-item-to-drag-to", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  await startCustomizing();
  let btn = document.getElementById("feed-button");
  let panel = document.getElementById(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  let placements = getAreaWidgetIds(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  let placementsAfterAppend = placements.concat(["feed-button"]);
  simulateItemDrag(btn, panel);
  assertAreaPlacements(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(btn, palette);
  CustomizableUI.removeWidgetFromArea("cui-panel-item-to-drag-to");
  ok(CustomizableUI.inDefaultState, "Should be in default state again.");
});

// Dragging an item from the palette to an empty panel should also work.
add_task(async function() {
  let widgetIds = getAreaWidgetIds(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  while (widgetIds.length) {
    CustomizableUI.removeWidgetFromArea(widgetIds.shift());
  }
  await startCustomizing();
  let btn = document.getElementById("feed-button");
  let panel = document.getElementById(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

  assertAreaPlacements(panel.id, []);

  let placementsAfterAppend = ["feed-button"];
  simulateItemDrag(btn, panel);
  assertAreaPlacements(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL, placementsAfterAppend);
  ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(btn, palette);
  assertAreaPlacements(panel.id, []);
});

registerCleanupFunction(async function asyncCleanup() {
  CustomizableUI.destroyWidget("cui-panel-item-to-drag-to");
  await endCustomizing();
  await resetCustomization();
});
