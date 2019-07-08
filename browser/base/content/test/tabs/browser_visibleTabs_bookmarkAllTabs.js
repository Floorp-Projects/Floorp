/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;
  is(gBrowser.visibleTabs.length, 1, "1 tab should be open");

  // Add a tab
  let testTab1 = BrowserTestUtils.addTab(gBrowser);
  is(gBrowser.visibleTabs.length, 2, "2 tabs should be open");

  let testTab2 = BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  is(gBrowser.visibleTabs.length, 3, "3 tabs should be open");
  // Wait for tab load, the code checks for currentURI.
  testTab2.linkedBrowser.addEventListener(
    "load",
    function() {
      // Hide the original tab
      gBrowser.selectedTab = testTab2;
      gBrowser.showOnlyTheseTabs([testTab2]);
      is(gBrowser.visibleTabs.length, 1, "1 tab should be visible");

      // Add a tab that will get pinned
      let pinned = BrowserTestUtils.addTab(gBrowser);
      is(gBrowser.visibleTabs.length, 2, "2 tabs should be visible now");
      gBrowser.pinTab(pinned);
      is(
        BookmarkTabHidden(),
        false,
        "Bookmark Tab should be visible on a normal tab"
      );
      gBrowser.selectedTab = pinned;
      is(
        BookmarkTabHidden(),
        false,
        "Bookmark Tab should be visible on a pinned tab"
      );

      // Show all tabs
      let allTabs = Array.from(gBrowser.tabs);
      gBrowser.showOnlyTheseTabs(allTabs);

      // reset the environment
      gBrowser.removeTab(testTab2);
      gBrowser.removeTab(testTab1);
      gBrowser.removeTab(pinned);
      is(gBrowser.visibleTabs.length, 1, "only orig is left and visible");
      is(gBrowser.tabs.length, 1, "sanity check that it matches");
      is(gBrowser.selectedTab, origTab, "got the orig tab");
      is(origTab.hidden, false, "and it's not hidden -- visible!");
      finish();
    },
    { capture: true, once: true }
  );
}

function BookmarkTabHidden() {
  updateTabContextMenu();
  return document.getElementById("context_bookmarkTab").hidden;
}
