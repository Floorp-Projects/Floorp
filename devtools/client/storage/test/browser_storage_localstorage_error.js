/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that for pages where local/sessionStorage is not available (like about:home),
// the host still appears in the storage tree and no unhandled exception is thrown.

add_task(async function() {
  await openTabAndSetupStorage("about:home");

  const itemsToOpen = [
    ["localStorage", "about:home"],
    ["sessionStorage", "about:home"]
  ];

  for (const item of itemsToOpen) {
    await selectTreeItem(item);
    ok(gUI.tree.isSelected(item), `Item ${item.join(" > ")} is present in the tree`);
  }

  await finishTests();
});
