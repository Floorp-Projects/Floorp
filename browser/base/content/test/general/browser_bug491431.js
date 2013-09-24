/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let testPage = "data:text/plain,test bug 491431 Page";

function test() {
  waitForExplicitFinish();

  let newWin, tabA, tabB;

  // test normal close
  tabA = gBrowser.addTab(testPage);
  gBrowser.tabContainer.addEventListener("TabClose", function(aEvent) {
    gBrowser.tabContainer.removeEventListener("TabClose", arguments.callee, true);
    ok(!aEvent.detail, "This was a normal tab close");

    // test tab close by moving
    tabB = gBrowser.addTab(testPage);
    gBrowser.tabContainer.addEventListener("TabClose", function(aEvent) {
      gBrowser.tabContainer.removeEventListener("TabClose", arguments.callee, true);
      executeSoon(function() {
        ok(aEvent.detail, "This was a tab closed by moving");

        // cleanup
        newWin.close();
        executeSoon(finish);
      });
    }, true);
    newWin = gBrowser.replaceTabWithWindow(tabB);
  }, true);
  gBrowser.removeTab(tabA);
}

