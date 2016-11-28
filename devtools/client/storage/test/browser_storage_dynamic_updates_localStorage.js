/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for localStorage.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");

  yield checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls4", "ls5", "ls6", "ls7"]
    ],
  ]);

  gWindow.localStorage.removeItem("ls4");

  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls5", "ls6", "ls7"]
    ],
  ]);

  gWindow.localStorage.setItem("ls4", "again");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls4", "ls5", "ls6", "ls7"]
    ],
  ]);
  // Updating a row
  gWindow.localStorage.setItem("ls2", "ls2-changed");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  checkCell("ls2", "value", "ls2-changed");

  // Clearing items. Bug 1233497 makes it so that we can no longer yield
  // CPOWs from Tasks. We work around this by calling clear via a ContentTask
  // instead.
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    return Task.spawn(content.wrappedJSObject.clear);
  });

  yield gUI.once("store-objects-cleared");

  yield checkState([
    [
      ["localStorage", "http://test1.example.org"],
      [ ]
    ],
  ]);

  yield finishTests();
});
