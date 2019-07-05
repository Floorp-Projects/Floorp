/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestToolbarId = "test-empty-drag";

// Attempting to drag an item to an empty container should work.
add_task(async function() {
  await createToolbarWithPlacements(kTestToolbarId, []);
  await startCustomizing();
  let libraryButton = document.getElementById("library-button");
  let customToolbar = document.getElementById(kTestToolbarId);
  simulateItemDrag(libraryButton, customToolbar);
  assertAreaPlacements(kTestToolbarId, ["library-button"]);
  ok(
    libraryButton.parentNode &&
      libraryButton.parentNode.parentNode == customToolbar,
    "Button should really be in toolbar"
  );
  await endCustomizing();
  removeCustomToolbars();
});

add_task(async function asyncCleanup() {
  await endCustomizing();
  await resetCustomization();
});
