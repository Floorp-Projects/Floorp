/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const remoteClientsFixture = [
  { id: 1, name: "Foo" },
  { id: 2, name: "Bar" },
];

add_task(async function test() {
  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
  let testTab = BrowserTestUtils.addTab(gBrowser);
  is(gBrowser.visibleTabs.length, 2, "there are now two visible tabs");

  // Check the context menu with two tabs
  updateTabContextMenu(origTab);
  is(
    document.getElementById("context_closeTab").disabled,
    false,
    "Close Tab is enabled"
  );

  // Hide the original tab.
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 1, "now there is only one visible tab");

  // Check the context menu with one tab.
  updateTabContextMenu(testTab);
  is(
    document.getElementById("context_closeTab").disabled,
    false,
    "Close Tab is enabled when more than one tab exists"
  );

  // Add a tab that will get pinned
  // So now there's one pinned tab, one visible unpinned tab, and one hidden tab
  let pinned = BrowserTestUtils.addTab(gBrowser);
  gBrowser.pinTab(pinned);
  is(gBrowser.visibleTabs.length, 2, "now there are two visible tabs");

  // Check the context menu on the pinned tab
  updateTabContextMenu(pinned);
  ok(
    !document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is enabled on pinned tab"
  );
  ok(
    !document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is enabled on pinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is disabled on pinned tab"
  );
  ok(
    !document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is enabled on pinned tab"
  );

  // Check the context menu on the unpinned visible tab
  updateTabContextMenu(testTab);
  ok(
    document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is disabled on single unpinned tab"
  );

  // Show all tabs
  let allTabs = Array.from(gBrowser.tabs);
  gBrowser.showOnlyTheseTabs(allTabs);

  // Check the context menu now
  updateTabContextMenu(testTab);
  ok(
    !document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is enabled on unpinned tab when there's another unpinned tab"
  );
  ok(
    !document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is enabled on unpinned tab when there's another unpinned tab"
  );
  ok(
    !document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is enabled on last unpinned tab when there's another unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is disabled on last unpinned tab"
  );

  // Check the context menu of the original tab
  // Close Tabs To The End should now be enabled
  updateTabContextMenu(origTab);
  ok(
    !document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is enabled on unpinned tab when followed by another"
  );

  gBrowser.removeTab(testTab);
  gBrowser.removeTab(pinned);
});
