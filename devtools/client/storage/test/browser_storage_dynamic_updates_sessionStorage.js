/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for sessionStorage.

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");

  await checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss1", "ss2", "ss3"],
    ],
  ]);

  await setSessionStorageItem("ss4", "new-item");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss1", "ss2", "ss3", "ss4"],
    ],
  ]);

  // deleting item

  await removeSessionStorageItem("ss3");

  await gUI.once("store-objects-edit");

  await removeSessionStorageItem("ss1");

  await gUI.once("store-objects-edit");

  await checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss2", "ss4"],
    ],
  ]);

  await selectTableItem("ss2");

  ok(!gUI.sidebar.hidden, "sidebar is visible");

  // Checking for correct value in sidebar before update
  await findVariableViewProperties([{name: "ss2", value: "foobar"}]);

  await setSessionStorageItem("ss2", "changed=ss2");

  await gUI.once("sidebar-updated");

  checkCell("ss2", "value", "changed=ss2");

  await findVariableViewProperties([{name: "ss2", value: "changed=ss2"}]);

  // Clearing items.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.clear();
  });

  await gUI.once("store-objects-cleared");

  await checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      [ ],
    ],
  ]);

  await finishTests();
});

async function setSessionStorageItem(key, value) {
  await ContentTask.spawn(gBrowser.selectedBrowser, [key, value],
    ([innerKey, innerValue]) => {
      content.wrappedJSObject.sessionStorage.setItem(innerKey, innerValue);
    }
  );
}

async function removeSessionStorageItem(key) {
  await ContentTask.spawn(gBrowser.selectedBrowser, key,
    innerKey => {
      content.wrappedJSObject.sessionStorage.removeItem(innerKey);
    }
  );
}
