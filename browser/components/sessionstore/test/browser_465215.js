/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 465215 **/

  waitForExplicitFinish();

  let uniqueName = "bug 465215";
  let uniqueValue1 = "as good as unique: " + Date.now();
  let uniqueValue2 = "as good as unique: " + Math.random();

  // set a unique value on a new, blank tab
  let tab1 = gBrowser.addTab();
  whenBrowserLoaded(tab1.linkedBrowser, function() {
    ss.setTabValue(tab1, uniqueName, uniqueValue1);

    // duplicate the tab with that value
    let tab2 = ss.duplicateTab(window, tab1);
    is(ss.getTabValue(tab2, uniqueName), uniqueValue1, "tab value was duplicated");

    ss.setTabValue(tab2, uniqueName, uniqueValue2);
    isnot(ss.getTabValue(tab1, uniqueName), uniqueValue2, "tab values aren't sync'd");

    // overwrite the tab with the value which should remove it
    ss.setTabState(tab1, JSON.stringify({ entries: [] }));
    whenTabRestored(tab1, function() {
      is(ss.getTabValue(tab1, uniqueName), "", "tab value was cleared");

      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab1);
      finish();
    });
  });
}
