/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // Open 2 other tabs, and pin the second one. Like that, the initial tab
  // should get closed.
  let testTab1 = gBrowser.addTab();
  let testTab2 = gBrowser.addTab();
  gBrowser.pinTab(testTab2);

  // Now execute "Close other Tabs" on the first manually opened tab (tab1).
  // -> tab2 ist pinned, tab1 should remain open and the initial tab should
  // get closed.
  gBrowser.removeAllTabsBut(testTab1);

  is(gBrowser.tabs.length, 2, "there are two remaining tabs open");
  is(gBrowser.tabs[0], testTab2, "pinned tab2 stayed open");
  is(gBrowser.tabs[1], testTab1, "tab1 stayed open");
  
  // Cleanup. Close only one tab because we need an opened tab at the end of
  // the test.
  gBrowser.removeTab(testTab2);
}
