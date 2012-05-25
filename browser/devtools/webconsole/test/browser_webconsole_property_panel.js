/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the functionality of the "property panel", which allows JavaScript
// objects and DOM nodes to be inspected.

const TEST_URI = "data:text/html;charset=utf8,<p>property panel test";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testPropertyPanel);
  }, true);
}

function testPropertyPanel(hud) {
  let jsterm = hud.jsterm;

  let propPanel = jsterm.openPropertyPanel({
    data: {
      object: [
        1,
        /abc/,
        null,
        undefined,
        function test() {},
        {}
      ]
    }
  });
  is (propPanel.treeView.rowCount, 6, "six elements shown in propertyPanel");
  propPanel.destroy();

  propPanel = jsterm.openPropertyPanel({
    data: {
      object: {
        "0.02": 0,
        "0.01": 1,
        "02":   2,
        "1":    3,
        "11":   4,
        "1.2":  5,
        "1.1":  6,
        "foo":  7,
        "bar":  8
      }
    }
  });
  is (propPanel.treeView.rowCount, 9, "nine elements shown in propertyPanel");

  let view = propPanel.treeView;
  is (view.getCellText(0), "0.01: 1", "1. element is okay");
  is (view.getCellText(1), "0.02: 0", "2. element is okay");
  is (view.getCellText(2), "1: 3",    "3. element is okay");
  is (view.getCellText(3), "1.1: 6",  "4. element is okay");
  is (view.getCellText(4), "1.2: 5",  "5. element is okay");
  is (view.getCellText(5), "02: 2",   "6. element is okay");
  is (view.getCellText(6), "11: 4",   "7. element is okay");
  is (view.getCellText(7), "bar: 8",  "8. element is okay");
  is (view.getCellText(8), "foo: 7",  "9. element is okay");
  propPanel.destroy();

  executeSoon(finishTest);
}

