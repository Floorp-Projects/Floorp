/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the functionality of the "property panel", which allows JavaScript
// objects and DOM nodes to be inspected.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testPropertyPanel, false);
}

function testPropertyPanel() {
  browser.removeEventListener("DOMContentLoaded", testPropertyPanel, false);

  openConsole();

  var jsterm = HUDService.getHudByWindow(content).jsterm;

  let propPanel = jsterm.openPropertyPanel("Test", [
    1,
    /abc/,
    null,
    undefined,
    function test() {},
    {}
  ]);
  is (propPanel.treeView.rowCount, 6, "six elements shown in propertyPanel");
  propPanel.destroy();

  propPanel = jsterm.openPropertyPanel("Test2", {
    "0.02": 0,
    "0.01": 1,
    "02":   2,
    "1":    3,
    "11":   4,
    "1.2":  5,
    "1.1":  6,
    "foo":  7,
    "bar":  8
  });
  is (propPanel.treeView.rowCount, 9, "nine elements shown in propertyPanel");

  let treeRows = propPanel.treeView._rows;
  is (treeRows[0].display, "0.01: 1", "1. element is okay");
  is (treeRows[1].display, "0.02: 0", "2. element is okay");
  is (treeRows[2].display, "1: 3",    "3. element is okay");
  is (treeRows[3].display, "1.1: 6",  "4. element is okay");
  is (treeRows[4].display, "1.2: 5",  "5. element is okay");
  is (treeRows[5].display, "02: 2",   "6. element is okay");
  is (treeRows[6].display, "11: 4",   "7. element is okay");
  is (treeRows[7].display, "bar: 8",  "8. element is okay");
  is (treeRows[8].display, "foo: 7",  "9. element is okay");
  propPanel.destroy();

  finishTest();
}

