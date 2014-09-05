/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to verify that the sidebar opens, closes and updates
// This test is not testing the values in the sidebar, being tested in _values

// Format: [
//   <id of the table item to click> or <id array for tree item to select> or
//     null to press Escape,
//   <do we wait for the async "sidebar-updated" event>,
//   <is the sidebar open>
// ]
const testCases = [
  [["cookies", "sectest1.example.org"], 0, 0],
  ["cs2", 1, 1],
  [null, 0, 0],
  ["cs2", 1, 1],
  ["uc1", 1, 1],
  ["uc1", 0, 1],
  [["localStorage", "http://sectest1.example.org"], 0, 0],
  ["iframe-u-ls1", 1, 1],
  ["iframe-u-ls1", 0, 1],
  [null, 0, 0],
  [["sessionStorage", "http://test1.example.org"], 0, 0],
  ["ss1", 1, 1],
  [null, 0, 0],
  [["indexedDB", "http://test1.example.org"], 0, 0],
  ["idb2", 1, 1],
  [["indexedDB", "http://test1.example.org", "idb2", "obj3"], 0, 0],
  [["indexedDB", "https://sectest1.example.org", "idb-s2"], 0, 0],
  ["obj-s2", 1, 1],
  [null, 0, 0],
  [null, 0, 0],
  ["obj-s2", 1, 1],
  [null, 0, 0],
];

let testSidebar = Task.async(function*() {
  let doc = gPanelWindow.document;
  for (let item of testCases) {
    info("clicking for item " + item);
    if (Array.isArray(item[0])) {
      selectTreeItem(item[0]);
      yield gUI.once("store-objects-updated");
    }
    else if (item[0]) {
      selectTableItem(item[0]);
    }
    else {
      EventUtils.sendKey("ESCAPE", gPanelWindow);
    }
    if (item[1]) {
      yield gUI.once("sidebar-updated");
    }
    is(!item[2], gUI.sidebar.hidden, "Correct visibility state of sidebar");
  }
});

let startTest = Task.async(function*() {
  yield testSidebar();
  finishTests();
});

function test() {
  openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html").then(startTest);
}
