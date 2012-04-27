/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Tests the console history feature accessed via the up and down arrow keys.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testHistory, false);
}

function testHistory() {
  browser.removeEventListener("DOMContentLoaded", testHistory, false);

  openConsole();

  let jsterm = HUDService.getHudByWindow(content).jsterm;
  let input = jsterm.inputNode;

  let executeList = ["document", "window", "window.location"];

  for each (var item in executeList) {
    input.value = item;
    jsterm.execute();
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

  jsterm.clearOutput();
  jsterm.history.splice(0, jsterm.history.length);   // workaround for bug 592552

  finishTest();
}

