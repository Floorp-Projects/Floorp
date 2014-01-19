/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestToolbarId = "test-empty-drag";

// Attempting to drag an item to an empty container should work.
add_task(function() {
  yield createToolbarWithPlacements(kTestToolbarId, []);
  yield startCustomizing();
  let downloadButton = document.getElementById("downloads-button");
  let customToolbar = document.getElementById(kTestToolbarId);
  simulateItemDrag(downloadButton, customToolbar);
  assertAreaPlacements(kTestToolbarId, ["downloads-button"]);
  ok(downloadButton.parentNode && downloadButton.parentNode.parentNode == customToolbar,
     "Button should really be in toolbar");
  yield endCustomizing();
  removeCustomToolbars();
});

add_task(function asyncCleanup() {
  yield endCustomizing();
  yield resetCustomization();
});
