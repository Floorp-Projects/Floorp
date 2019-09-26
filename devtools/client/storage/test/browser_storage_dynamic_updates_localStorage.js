/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for localStorage.

add_task(async function() {
  const TEST_HOST = "http://test1.example.org";

  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");

  const expectedKeys = ["1", "2", "3", "4", "5", "null", "non-json-parsable"];

  // Test on string keys that JSON.parse can parse without throwing
  // (to verify the issue fixed by Bug 1578447 doesn't regress).
  await testRemoveAndChange("null", expectedKeys, TEST_HOST);
  await testRemoveAndChange("4", expectedKeys, TEST_HOST);
  // Test on a string that makes JSON.parse to throw.
  await testRemoveAndChange("non-json-parsable", expectedKeys, TEST_HOST);

  // Clearing items.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.clear();
  });

  await gUI.once("store-objects-cleared");

  await checkState([[["localStorage", TEST_HOST], []]]);

  await finishTests();
});

async function testRemoveAndChange(targetKey, expectedKeys, host) {
  await checkState([[["localStorage", host], expectedKeys]]);

  await removeLocalStorageItem(targetKey);
  await gUI.once("store-objects-edit");
  await checkState([
    [["localStorage", host], expectedKeys.filter(key => key !== targetKey)],
  ]);

  await setLocalStorageItem(targetKey, "again");
  await gUI.once("store-objects-edit");
  await checkState([[["localStorage", host], expectedKeys]]);

  // Updating a row set to the string "null"
  await setLocalStorageItem(targetKey, `key-${targetKey}-changed`);
  await gUI.once("store-objects-edit");
  checkCell(targetKey, "value", `key-${targetKey}-changed`);
}

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
