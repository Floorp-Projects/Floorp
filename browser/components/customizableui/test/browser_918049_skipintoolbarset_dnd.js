/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var navbar;
var skippedItem;

// Attempting to drag a skipintoolbarset item should work.
add_task(async function() {
  navbar = document.getElementById("nav-bar");
  skippedItem = document.createXULElement("toolbarbutton");
  skippedItem.id = "test-skipintoolbarset-item";
  skippedItem.setAttribute("label", "Test");
  skippedItem.setAttribute("skipintoolbarset", "true");
  skippedItem.setAttribute("removable", "true");
  CustomizableUI.getCustomizationTarget(navbar).appendChild(skippedItem);
  let libraryButton = document.getElementById("library-button");
  await startCustomizing();
  await waitForElementShown(skippedItem);
  ok(CustomizableUI.inDefaultState, "Should still be in default state");
  simulateItemDrag(skippedItem, libraryButton, "start");
  ok(CustomizableUI.inDefaultState, "Should still be in default state");
  let skippedItemWrapper = skippedItem.parentNode;
  is(
    skippedItemWrapper.nextElementSibling &&
      skippedItemWrapper.nextElementSibling.id,
    libraryButton.parentNode.id,
    "Should be next to library button"
  );
  simulateItemDrag(libraryButton, skippedItem, "start");
  let libraryWrapper = libraryButton.parentNode;
  is(
    libraryWrapper.nextElementSibling && libraryWrapper.nextElementSibling.id,
    skippedItem.parentNode.id,
    "Should be next to skipintoolbarset item"
  );
  ok(CustomizableUI.inDefaultState, "Should still be in default state");
});

add_task(async function asyncCleanup() {
  await endCustomizing();
  skippedItem.remove();
  await resetCustomization();
});
