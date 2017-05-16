/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testPage = "data:text/plain,test bug 491431 Page";

function test() {
  waitForExplicitFinish();

  let newWin, tabA, tabB;

  // test normal close
  tabA = BrowserTestUtils.addTab(gBrowser, testPage);
  gBrowser.tabContainer.addEventListener("TabClose", function(firstTabCloseEvent) {
    ok(!firstTabCloseEvent.detail.adoptedBy, "This was a normal tab close");

    // test tab close by moving
    tabB = BrowserTestUtils.addTab(gBrowser, testPage);
    gBrowser.tabContainer.addEventListener("TabClose", function(secondTabCloseEvent) {
      executeSoon(function() {
        ok(secondTabCloseEvent.detail.adoptedBy, "This was a tab closed by moving");

        // cleanup
        newWin.close();
        executeSoon(finish);
      });
    }, {capture: true, once: true});
    newWin = gBrowser.replaceTabWithWindow(tabB);
  }, {capture: true, once: true});
  gBrowser.removeTab(tabA);
}

