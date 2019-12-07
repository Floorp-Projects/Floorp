/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

// Basic test to check the rapid adding and removing of localStorage entries.

"use strict";

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-blank.html");
  await selectTreeItem(["localStorage", "http://test1.example.org"]);

  ok(isTableEmpty(), "Table empty on init");

  for (let i = 0; i < 10; i++) {
    await addRemove(`test${i}`);
  }

  await finishTests();
});

function* addRemove(name) {
  yield ContentTask.spawn(gBrowser.selectedBrowser, name, innerName => {
    content.localStorage.setItem(innerName, "true");
    content.localStorage.removeItem(innerName);
  });

  info("Waiting for store objects to be changed");
  yield gUI.once("store-objects-edit");

  ok(isTableEmpty(), `Table empty after rapid add/remove of "${name}"`);
}
