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
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Paul O’Shannessy <paul@oshannessy.com>
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

const stateBackup = ss.getBrowserState();
const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank" }] },
      { entries: [{ url: "about:mozilla" }] }
    ]
  }]
};


function test() {
  /** Test for Bug 618151 - Overwriting state can lead to unrestored tabs **/
  waitForExplicitFinish();
  runNextTest();
}

// Just a subset of tests from bug 615394 that causes a timeout.
let tests = [test_setup, test_hang];
function runNextTest() {
  // set an empty state & run the next test, or finish
  if (tests.length) {
    // Enumerate windows and close everything but our primary window. We can't
    // use waitForFocus() because apparently it's buggy. See bug 599253.
    var windowsEnum = Services.wm.getEnumerator("navigator:browser");
    while (windowsEnum.hasMoreElements()) {
      var currentWindow = windowsEnum.getNext();
      if (currentWindow != window) {
        currentWindow.close();
      }
    }

    let currentTest = tests.shift();
    info("running " + currentTest.name);
    waitForBrowserState(testState, currentTest);
  }
  else {
    ss.setBrowserState(stateBackup);
    executeSoon(finish);
  }
}

function test_setup() {
  function onSSTabRestored(aEvent) {
    gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, false);
    runNextTest();
  }

  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, false);
  ss.setTabState(gBrowser.tabs[1], JSON.stringify({
    entries: [{ url: "http://example.org" }],
    extData: { foo: "bar" } }));
}

function test_hang() {
  ok(true, "test didn't time out");
  runNextTest();
}
