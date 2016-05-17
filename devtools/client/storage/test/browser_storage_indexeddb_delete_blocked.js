/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test what happens when deleting indexedDB database is blocked

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-idb-delete-blocked.html");

  info("test state before delete");
  yield checkState([
    [["indexedDB", "http://test1.example.org"], ["idb"]]
  ]);

  info("do the delete");
  yield selectTreeItem(["indexedDB", "http://test1.example.org"]);
  let actor = gUI.getCurrentActor();
  let result = yield actor.removeDatabase("http://test1.example.org", "idb");

  ok(result.blocked, "removeDatabase attempt is blocked");

  info("test state after blocked delete");
  yield checkState([
    [["indexedDB", "http://test1.example.org"], ["idb"]]
  ]);

  let eventWait = gUI.once("store-objects-updated");

  info("telling content to close the db");
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    let win = content.wrappedJSObject;
    yield win.closeDb();
  });

  info("waiting for store update events");
  yield eventWait;

  info("test state after real delete");
  yield checkState([
    [["indexedDB", "http://test1.example.org"], []]
  ]);

  info("try to delete database from nonexistent host");
  let errorThrown = false;
  try {
    result = yield actor.removeDatabase("http://test2.example.org", "idb");
  } catch (ex) {
    errorThrown = true;
  }

  ok(errorThrown, "error was reported when trying to delete");

  yield finishTests();
});
