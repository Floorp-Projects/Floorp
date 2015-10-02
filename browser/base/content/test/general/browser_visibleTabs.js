/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* () {
  // Ensure TabView has been initialized already. Otherwise it could
  // activate at an unexpected time and show/hide tabs.
  yield new Promise(resolve => TabView._initFrame(resolve));

  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;

  // Add a tab that will get pinned
  let pinned = gBrowser.addTab();
  gBrowser.pinTab(pinned);

  let testTab = gBrowser.addTab();

  let visible = gBrowser.visibleTabs;
  is(visible.length, 3, "3 tabs should be open");
  is(visible[0], pinned, "the pinned tab is first");
  is(visible[1], origTab, "original tab is next");
  is(visible[2], testTab, "last created tab is last");

  // Only show the test tab (but also get pinned and selected)
  is(gBrowser.selectedTab, origTab, "sanity check that we're on the original tab");
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are still visible");

  // Select the test tab and only show that (and pinned)
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);

  // if the tabview frame is initialized, we need to move the orignal tab to
  // another group; otherwise, selecting a tab would make all three tabs in 
  // the same group to display.
  let tabViewWindow = TabView.getContentWindow();
  if (tabViewWindow)
    tabViewWindow.GroupItems.moveTabToGroupItem(origTab, null);

  visible = gBrowser.visibleTabs;
  is(visible.length, 2, "2 tabs should be visible including the pinned");
  is(visible[0], pinned, "first is pinned");
  is(visible[1], testTab, "next is the test tab");
  is(gBrowser.tabs.length, 3, "3 tabs should still be open");

  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "first tab is pinned");
  gBrowser.selectTabAtIndex(1);
  is(gBrowser.selectedTab, testTab, "second tab is the test tab");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, testTab, "no third tab, so no change");
  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "switch back to the pinned");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, pinned, "no third tab, so no change");
  gBrowser.selectTabAtIndex(-1);
  is(gBrowser.selectedTab, testTab, "last tab is the test tab");

  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "wrapped around the end to pinned");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned again");

  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "going backwards to last tab");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab again");

  // Try showing all tabs
  gBrowser.showOnlyTheseTabs(Array.slice(gBrowser.tabs));
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are visible again");

  // Select the pinned tab and show the testTab to make sure selection updates
  gBrowser.selectedTab = pinned;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.tabs[1], origTab, "make sure origTab is in the middle");
  is(origTab.hidden, true, "make sure it's hidden");
  gBrowser.removeTab(pinned);
  is(gBrowser.selectedTab, testTab, "making sure origTab was skipped");
  is(gBrowser.visibleTabs.length, 1, "only testTab is there");

  // Only show one of the non-pinned tabs (but testTab is selected)
  gBrowser.showOnlyTheseTabs([origTab]);
  is(gBrowser.visibleTabs.length, 2, "got 2 tabs");

  // Now really only show one of the tabs
  gBrowser.showOnlyTheseTabs([testTab]);
  visible = gBrowser.visibleTabs;
  is(visible.length, 1, "only the original tab is visible");
  is(visible[0], testTab, "it's the original tab");
  is(gBrowser.tabs.length, 2, "still have 2 open tabs");

  // Close the last visible tab and make sure we still get a visible tab
  gBrowser.removeTab(testTab);
  is(gBrowser.visibleTabs.length, 1, "only orig is left and visible");
  is(gBrowser.tabs.length, 1, "sanity check that it matches");
  is(gBrowser.selectedTab, origTab, "got the orig tab");
  is(origTab.hidden, false, "and it's not hidden -- visible!");

  if (tabViewWindow)
    tabViewWindow.GroupItems.groupItems[0].close();
});

