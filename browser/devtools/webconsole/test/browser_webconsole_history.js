/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the console history feature accessed via the up and down arrow keys.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

let test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  let executeList = ["document", "window", "window.location"];

  for (var item of executeList) {
    input.value = item;
    yield jsterm.execute();
  }

  for (var i = executeList.length - 1; i != -1; i--) {
    jsterm.historyPeruse(HISTORY_BACK);
    is (input.value, executeList[i], "check history previous idx:" + i);
  }

  jsterm.historyPeruse(HISTORY_BACK);
  is (input.value, executeList[0], "test that item is still index 0");

  jsterm.historyPeruse(HISTORY_BACK);
  is (input.value, executeList[0], "test that item is still still index 0");

  for (var i = 1; i < executeList.length; i++) {
    jsterm.historyPeruse(HISTORY_FORWARD);
    is (input.value, executeList[i], "check history next idx:" + i);
  }

  jsterm.historyPeruse(HISTORY_FORWARD);
  is (input.value, "", "check input is empty again");

  // Simulate pressing Arrow_Down a few times and then if Arrow_Up shows
  // the previous item from history again.
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);

  is (input.value, "", "check input is still empty");

  let idxLast = executeList.length - 1;
  jsterm.historyPeruse(HISTORY_BACK);
  is (input.value, executeList[idxLast], "check history next idx:" + idxLast);
});
