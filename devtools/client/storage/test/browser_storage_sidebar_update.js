/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar is not broken when several updates
// come in quick succession. See bug 1260380 - it could happen that the
// "Parsed Value" section gets duplicated.

"use strict";

add_task(async function() {
  const ITEM_NAME = "ls1";
  const UPDATE_COUNT = 3;

  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-complex-values.html");

  const updated = gUI.once("sidebar-updated");
  await selectTreeItem(["localStorage", "http://test1.example.org"]);
  await selectTableItem(ITEM_NAME);
  await updated;

  is(gUI.sidebar.hidden, false, "sidebar is visible");

  // do several updates in a row and wait for them to finish
  const updates = [];
  for (let i = 0; i < UPDATE_COUNT; i++) {
    info(`Performing update #${i}`);
    updates.push(gUI.once("sidebar-updated"));
    gUI.updateObjectSidebar();
  }
  await Promise.all(updates);

  info("Updates performed, going to verify result");
  const parsedScope = gUI.view.getScopeAtIndex(1);
  const elements = parsedScope.target.querySelectorAll(
    `.name[value="${ITEM_NAME}"]`
  );
  is(
    elements.length,
    1,
    `There is only one displayed variable named '${ITEM_NAME}'`
  );
});
