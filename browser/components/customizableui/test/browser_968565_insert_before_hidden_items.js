/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kHidden1Id = "test-hidden-button-1";
const kHidden2Id = "test-hidden-button-2";

var navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);

// When we drag an item onto a customizable area, and not over a specific target, we
// should assume that we're appending them to the area. If doing so, we should scan
// backwards over any hidden items and insert the item before those hidden items.
add_task(async function() {
  ok(CustomizableUI.inDefaultState, "Should be in the default state");

  // Iterate backwards over the items in the nav-bar until we find the first
  // one that is not hidden.
  let placements = CustomizableUI.getWidgetsInArea(CustomizableUI.AREA_NAVBAR);
  let lastVisible = null;
  for (let widgetGroup of placements.reverse()) {
    let widget = widgetGroup.forWindow(window);
    if (widget && widget.node && !widget.node.hidden) {
      lastVisible = widget.node;
      break;
    }
  }

  if (!lastVisible) {
    ok(false, "Apparently, there are no visible items in the nav-bar.");
  }

  info("The last visible item in the nav-bar has ID: " + lastVisible.id);

  let hidden1 = createDummyXULButton(kHidden1Id, "You can't see me");
  let hidden2 = createDummyXULButton(kHidden2Id, "You can't see me either.");
  hidden1.hidden = hidden2.hidden = true;

  // Make sure we have some hidden items at the end of the nav-bar.
  navbar.insertItem(hidden1.id);
  navbar.insertItem(hidden2.id);

  // Drag an item and drop it onto the nav-bar customization target, but
  // not over a particular item.
  await startCustomizing();
  let homeButton = document.getElementById("home-button");
  simulateItemDrag(homeButton, navbar.customizationTarget, "end");

  await endCustomizing();

  is(homeButton.previousSibling.id, lastVisible.id,
     "The downloads button should be placed after the last visible item.");

  await resetCustomization();
});
