/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar is not broken when several updates
// come in quick succession. See bug 1260380 - it could happen that the
// "Parsed Value" section gets duplicated.

"use strict";

add_task(function* () {
  const ITEM_NAME = "ls1";
  const UPDATE_COUNT = 3;

  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-complex-values.html");

  let updated = gUI.once("sidebar-updated");
  yield selectTreeItem(["localStorage", "http://test1.example.org"]);
  yield selectTableItem(ITEM_NAME);
  yield updated;

  is(gUI.sidebar.hidden, false, "sidebar is visible");

  // do several updates in a row and wait for them to finish
  let updates = [];
  for (let i = 0; i < UPDATE_COUNT; i++) {
    info(`Performing update #${i}`);
    updates.push(gUI.once("sidebar-updated"));
    gUI.updateObjectSidebar();
  }
  yield promise.all(updates);

  info("Updates performed, going to verify result");
  let parsedScope = gUI.view.getScopeAtIndex(1);
  let elements = parsedScope.target.querySelectorAll(
    `.name[value="${ITEM_NAME}"]`);
  is(elements.length, 1,
    `There is only one displayed variable named '${ITEM_NAME}'`);

  yield finishTests();
});
