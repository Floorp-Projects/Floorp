/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let draggedItem;

/**
 * Check that customizing-movingItem gets removed on a drop when the item is moved.
 */

// Drop on the palette
add_task(function() {
  draggedItem = document.createElement("toolbarbutton");
  draggedItem.id = "test-dragEnd-after-move1";
  draggedItem.setAttribute("label", "Test");
  draggedItem.setAttribute("removable", "true");
  let navbar = document.getElementById("nav-bar");
  navbar.customizationTarget.appendChild(draggedItem);
  yield startCustomizing();
  simulateItemDrag(draggedItem, gCustomizeMode.visiblePalette);
  is(document.documentElement.hasAttribute("customizing-movingItem"), false,
     "Make sure customizing-movingItem is removed after dragging to the palette");
});

// Drop on a customization target itself
add_task(function() {
  draggedItem = document.createElement("toolbarbutton");
  draggedItem.id = "test-dragEnd-after-move2";
  draggedItem.setAttribute("label", "Test");
  draggedItem.setAttribute("removable", "true");
  let dest = createToolbarWithPlacements("test-dragEnd");
  let navbar = document.getElementById("nav-bar");
  navbar.customizationTarget.appendChild(draggedItem);
  yield startCustomizing();
  simulateItemDrag(draggedItem, dest.customizationTarget);
  is(document.documentElement.hasAttribute("customizing-movingItem"), false,
     "Make sure customizing-movingItem is removed");
});

add_task(function asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});
