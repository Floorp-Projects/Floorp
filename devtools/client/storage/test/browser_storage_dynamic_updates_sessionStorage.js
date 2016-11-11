/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test dynamic updates in the storage inspector for sessionStorage.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-updates.html");

  gUI.tree.expandAll();

  ok(gUI.sidebar.hidden, "Sidebar is initially hidden");

  yield checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss1", "ss2", "ss3"]
    ],
  ]);

  gWindow.sessionStorage.setItem("ss4", "new-item");

  yield gUI.once("store-objects-updated");
  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss1", "ss2", "ss3", "ss4"]
    ],
  ]);

  // deleting item

  gWindow.sessionStorage.removeItem("ss3");

  yield gUI.once("store-objects-updated");

  gWindow.sessionStorage.removeItem("ss1");

  yield gUI.once("store-objects-updated");

  yield checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      ["ss2", "ss4"]
    ],
  ]);

  yield selectTableItem("ss2");

  ok(!gUI.sidebar.hidden, "sidebar is visible");

  // Checking for correct value in sidebar before update
  yield findVariableViewProperties([{name: "ss2", value: "foobar"}]);

  gWindow.sessionStorage.setItem("ss2", "changed=ss2");

  yield gUI.once("sidebar-updated");

  checkCell("ss2", "value", "changed=ss2");

  yield findVariableViewProperties([{name: "ss2", value: "changed=ss2"}]);

  // Clearing items. Bug 1233497 makes it so that we can no longer yield
  // CPOWs from Tasks. We work around this by calling clear via a ContentTask
  // instead.
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    return Task.spawn(content.wrappedJSObject.clear);
  });

  yield gUI.once("store-objects-cleared");

  yield checkState([
    [
      ["sessionStorage", "http://test1.example.org"],
      [ ]
    ],
  ]);

  yield finishTests();
});
