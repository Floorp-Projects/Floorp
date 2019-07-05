/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for localStorage.

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");

  await checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls4", "ls5", "ls6", "ls7"],
    ],
  ]);

  await removeLocalStorageItem("ls4");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls5", "ls6", "ls7"],
    ],
  ]);

  await setLocalStorageItem("ls4", "again");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["localStorage", "http://test1.example.org"],
      ["ls1", "ls2", "ls3", "ls4", "ls5", "ls6", "ls7"],
    ],
  ]);
  // Updating a row
  await setLocalStorageItem("ls2", "ls2-changed");

  await gUI.once("store-objects-edit");

  checkCell("ls2", "value", "ls2-changed");

  // Clearing items.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.clear();
  });

  await gUI.once("store-objects-cleared");

  await checkState([[["localStorage", "http://test1.example.org"], []]]);

  await finishTests();
});

async function setLocalStorageItem(key, value) {
  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [key, value],
    ([innerKey, innerValue]) => {
      content.wrappedJSObject.localStorage.setItem(innerKey, innerValue);
    }
  );
}

async function removeLocalStorageItem(key) {
  await ContentTask.spawn(gBrowser.selectedBrowser, key, innerKey => {
    content.wrappedJSObject.localStorage.removeItem(innerKey);
  });
}
