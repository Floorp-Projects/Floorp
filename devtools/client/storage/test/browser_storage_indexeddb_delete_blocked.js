/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test what happens when deleting indexedDB database is blocked

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-idb-delete-blocked.html");

  info("test state before delete");
  await checkState([
    [["indexedDB", "http://test1.example.org"], ["idb (default)"]]
  ]);

  info("do the delete");
  await selectTreeItem(["indexedDB", "http://test1.example.org"]);
  const front = gUI.getCurrentFront();
  let result = await front.removeDatabase("http://test1.example.org", "idb (default)");

  ok(result.blocked, "removeDatabase attempt is blocked");

  info("test state after blocked delete");
  await checkState([
    [["indexedDB", "http://test1.example.org"], ["idb (default)"]]
  ]);

  const eventWait = gUI.once("store-objects-edit");

  info("telling content to close the db");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    const win = content.wrappedJSObject;
    await win.closeDb();
  });

  info("waiting for store edit events");
  await eventWait;

  info("test state after real delete");
  await checkState([
    [["indexedDB", "http://test1.example.org"], []]
  ]);

  info("try to delete database from nonexistent host");
  let errorThrown = false;
  try {
    result = await front.removeDatabase("http://test2.example.org", "idb (default)");
  } catch (ex) {
    errorThrown = true;
  }

  ok(errorThrown, "error was reported when trying to delete");

  await finishTests();
});
