/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function simulateItemDragAndEnd(aToDrag, aTarget) {
  var ds = Cc["@mozilla.org/widget/dragservice;1"].getService(
    Ci.nsIDragService
  );

  ds.startDragSessionForTests(
    Ci.nsIDragService.DRAGDROP_ACTION_MOVE |
      Ci.nsIDragService.DRAGDROP_ACTION_COPY |
      Ci.nsIDragService.DRAGDROP_ACTION_LINK
  );
  try {
    var [result, dataTransfer] = EventUtils.synthesizeDragOver(
      aToDrag.parentNode,
      aTarget
    );
    EventUtils.synthesizeDropAfterDragOver(result, dataTransfer, aTarget);
    // Send dragend to move dragging item back to initial place.
    EventUtils.sendDragEvent(
      { type: "dragend", dataTransfer },
      aToDrag.parentNode
    );
  } finally {
    ds.endDragSession(true);
  }
}

add_task(async function checkNoAddingToPanel() {
  let area = CustomizableUI.AREA_FIXED_OVERFLOW_PANEL;
  let previousPlacements = getAreaWidgetIds(area);
  CustomizableUI.addWidgetToArea("separator", area);
  CustomizableUI.addWidgetToArea("spring", area);
  CustomizableUI.addWidgetToArea("spacer", area);
  assertAreaPlacements(area, previousPlacements);

  let oldNumberOfItems = previousPlacements.length;
  if (getAreaWidgetIds(area).length != oldNumberOfItems) {
    CustomizableUI.reset();
  }
});

add_task(async function checkAddingToToolbar() {
  let area = CustomizableUI.AREA_NAVBAR;
  let previousPlacements = getAreaWidgetIds(area);
  CustomizableUI.addWidgetToArea("separator", area);
  CustomizableUI.addWidgetToArea("spring", area);
  CustomizableUI.addWidgetToArea("spacer", area);
  let expectedPlacements = [...previousPlacements].concat([
    /separator/,
    /spring/,
    /spacer/,
  ]);
  assertAreaPlacements(area, expectedPlacements);

  let newlyAddedElements = getAreaWidgetIds(area).slice(-3);
  while (newlyAddedElements.length) {
    CustomizableUI.removeWidgetFromArea(newlyAddedElements.shift());
  }

  assertAreaPlacements(area, previousPlacements);

  let oldNumberOfItems = previousPlacements.length;
  if (getAreaWidgetIds(area).length != oldNumberOfItems) {
    CustomizableUI.reset();
  }
});

add_task(async function checkDragging() {
  let startArea = CustomizableUI.AREA_TABSTRIP;
  let targetArea = CustomizableUI.AREA_FIXED_OVERFLOW_PANEL;
  let startingToolbarPlacements = getAreaWidgetIds(startArea);
  let startingTargetPlacements = getAreaWidgetIds(targetArea);

  CustomizableUI.addWidgetToArea("separator", startArea);
  CustomizableUI.addWidgetToArea("spring", startArea);
  CustomizableUI.addWidgetToArea("spacer", startArea);

  let placementsWithSpecials = getAreaWidgetIds(startArea);
  let elementsToMove = [];
  for (let id of placementsWithSpecials) {
    if (CustomizableUI.isSpecialWidget(id)) {
      elementsToMove.push(id);
    }
  }
  is(elementsToMove.length, 3, "Should have 3 elements to try and drag.");

  await startCustomizing();
  let existingSpecial = null;
  existingSpecial = gCustomizeMode.visiblePalette.querySelector(
    "toolbarspring"
  );
  ok(
    existingSpecial,
    "Should have a flexible space in the palette by default in photon"
  );
  for (let id of elementsToMove) {
    simulateItemDragAndEnd(
      document.getElementById(id),
      document.getElementById(targetArea)
    );
  }

  assertAreaPlacements(startArea, placementsWithSpecials);
  assertAreaPlacements(targetArea, startingTargetPlacements);

  for (let id of elementsToMove) {
    simulateItemDrag(
      document.getElementById(id),
      gCustomizeMode.visiblePalette
    );
  }

  assertAreaPlacements(startArea, startingToolbarPlacements);
  assertAreaPlacements(targetArea, startingTargetPlacements);

  let allSpecials = gCustomizeMode.visiblePalette.querySelectorAll(
    "toolbarspring,toolbarseparator,toolbarspacer"
  );
  allSpecials = [...allSpecials].filter(special => special != existingSpecial);
  ok(
    !allSpecials.length,
    "No (new) specials should make it to the palette alive."
  );
  await endCustomizing();
});

add_task(async function asyncCleanup() {
  await endCustomizing();
  CustomizableUI.reset();
});
