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
  let stopReloadButton = document.getElementById("stop-reload-button");
  await startCustomizing();
  await waitForElementShown(skippedItem);
  ok(CustomizableUI.inDefaultState, "Should still be in default state");
  simulateItemDrag(skippedItem, stopReloadButton, "start", 0);
  ok(CustomizableUI.inDefaultState, "Should still be in default state");
  let skippedItemWrapper = skippedItem.parentNode;
  is(
    skippedItemWrapper.nextElementSibling &&
      skippedItemWrapper.nextElementSibling.id,
    stopReloadButton.parentNode.id,
    "Should be next to stop/reload button"
  );
  simulateItemDrag(stopReloadButton, skippedItem, "start", 0);
  let wrapper = stopReloadButton.parentNode;
  is(
    wrapper.nextElementSibling && wrapper.nextElementSibling.id,
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
