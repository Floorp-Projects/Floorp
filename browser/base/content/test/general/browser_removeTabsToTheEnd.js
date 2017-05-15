/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // Add two new tabs after the original tab. Pin the first one.
  let originalTab = gBrowser.selectedTab;
  let newTab1 = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);
  gBrowser.pinTab(newTab1);

  // Check that there is only one closable tab from originalTab to the end
  is(gBrowser.getTabsToTheEndFrom(originalTab).length, 1,
    "One unpinned tab to the right");

  // Remove tabs to the end
  gBrowser.removeTabsToTheEndFrom(originalTab);
  is(gBrowser.tabs.length, 2, "Length is 2");
  is(gBrowser.tabs[1], originalTab, "Starting tab is not removed");
  is(gBrowser.tabs[0], newTab1, "Pinned tab is not removed");

  // Remove pinned tab
  gBrowser.removeTab(newTab1);
}
