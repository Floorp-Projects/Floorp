/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var draggedItem;

/**
 * Check that customizing-movingItem gets removed on a drop when the item is moved.
 */

// Drop on the palette
add_task(async function() {
  draggedItem = document.createXULElement("toolbarbutton");
  draggedItem.id = "test-dragEnd-after-move1";
  draggedItem.setAttribute("label", "Test");
  draggedItem.setAttribute("removable", "true");
  let navbar = document.getElementById("nav-bar");
  CustomizableUI.getCustomizationTarget(navbar).appendChild(draggedItem);
  await startCustomizing();
  simulateItemDrag(draggedItem, gCustomizeMode.visiblePalette);
  is(document.documentElement.hasAttribute("customizing-movingItem"), false,
     "Make sure customizing-movingItem is removed after dragging to the palette");
  await endCustomizing();
});

// Drop on a customization target itself
add_task(async function() {
  draggedItem = document.createXULElement("toolbarbutton");
  draggedItem.id = "test-dragEnd-after-move2";
  draggedItem.setAttribute("label", "Test");
  draggedItem.setAttribute("removable", "true");
  let dest = createToolbarWithPlacements("test-dragEnd");
  let navbar = document.getElementById("nav-bar");
  CustomizableUI.getCustomizationTarget(navbar).appendChild(draggedItem);
  await startCustomizing();
  simulateItemDrag(draggedItem, CustomizableUI.getCustomizationTarget(dest));
  is(document.documentElement.hasAttribute("customizing-movingItem"), false,
     "Make sure customizing-movingItem is removed");
  await endCustomizing();
});

registerCleanupFunction(async function asyncCleanup() {
  await endCustomizing();
  removeCustomToolbars();
});
