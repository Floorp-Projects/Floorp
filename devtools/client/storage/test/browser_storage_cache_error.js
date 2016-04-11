/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

// Test handling errors in CacheStorage

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cache-error.html");

  const cacheItemId = ["Cache", "javascript:parent.frameContent"];

  gUI.tree.selectedItem = cacheItemId;
  ok(gUI.tree.isSelected(cacheItemId),
    `The item ${cacheItemId.join(" > ")} is present in the tree`);

  yield finishTests();
});
