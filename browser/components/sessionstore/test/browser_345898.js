/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 345898 **/

  function test(aLambda) {
    try {
      aLambda();
      return false;
    }
    catch (ex) {
      return ex.name == "NS_ERROR_ILLEGAL_VALUE" ||
             ex.name == "NS_ERROR_FAILURE";
    }
  }

  // all of the following calls with illegal arguments should throw NS_ERROR_ILLEGAL_VALUE
  ok(test(() => ss.getWindowState({})),
     "Invalid window for getWindowState throws");
  ok(test(() => ss.setWindowState({}, "", false)),
     "Invalid window for setWindowState throws");
  ok(test(() => ss.getTabState({})),
     "Invalid tab for getTabState throws");
  ok(test(() => ss.setTabState({}, "{}")),
     "Invalid tab state for setTabState throws");
  ok(test(() => ss.setTabState({}, JSON.stringify({ entries: [] }))),
     "Invalid tab for setTabState throws");
  ok(test(() => ss.duplicateTab({}, {})),
     "Invalid tab for duplicateTab throws");
  ok(test(() => ss.duplicateTab({}, gBrowser.selectedTab)),
     "Invalid window for duplicateTab throws");
  ok(test(() => ss.getClosedTabData({})),
     "Invalid window for getClosedTabData throws");
  ok(test(() => ss.undoCloseTab({}, 0)),
     "Invalid window for undoCloseTab throws");
  ok(test(() => ss.undoCloseTab(window, -1)),
     "Invalid index for undoCloseTab throws");
  ok(test(() => ss.getWindowValue({}, "")),
     "Invalid window for getWindowValue throws");
  ok(test(() => ss.setWindowValue({}, "", "")),
     "Invalid window for setWindowValue throws");
}
