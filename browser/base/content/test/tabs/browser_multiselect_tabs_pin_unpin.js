add_task(async function test() {
  let tab1 = gBrowser.selectedTab;
  let tab2 = await addTab();
  let tab3 = await addTab();

  let menuItemPinTab = document.getElementById("context_pinTab");
  let menuItemUnpinTab = document.getElementById("context_unpinTab");
  let menuItemPinSelectedTabs = document.getElementById(
    "context_pinSelectedTabs"
  );
  let menuItemUnpinSelectedTabs = document.getElementById(
    "context_unpinSelectedTabs"
  );

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");

  // Check the context menu with a non-multiselected tab
  updateTabContextMenu(tab3);
  ok(!tab3.pinned, "Tab3 is unpinned");
  is(menuItemPinTab.hidden, false, "Pin Tab is visible");
  is(menuItemUnpinTab.hidden, true, "Unpin Tab is hidden");
  is(menuItemPinSelectedTabs.hidden, true, "Pin Selected Tabs is hidden");
  is(menuItemUnpinSelectedTabs.hidden, true, "Unpin Selected Tabs is hidden");

  // Check the context menu with a multiselected and unpinned tab
  updateTabContextMenu(tab2);
  ok(!tab2.pinned, "Tab2 is unpinned");
  is(menuItemPinTab.hidden, true, "Pin Tab is hidden");
  is(menuItemUnpinTab.hidden, true, "Unpin Tab is hidden");
  is(menuItemPinSelectedTabs.hidden, false, "Pin Selected Tabs is visible");
  is(menuItemUnpinSelectedTabs.hidden, true, "Unpin Selected Tabs is hidden");

  let tab1Pinned = BrowserTestUtils.waitForEvent(tab1, "TabPinned");
  let tab2Pinned = BrowserTestUtils.waitForEvent(tab2, "TabPinned");
  menuItemPinSelectedTabs.click();
  await tab1Pinned;
  await tab2Pinned;

  ok(tab1.pinned, "Tab1 is pinned");
  ok(tab2.pinned, "Tab2 is pinned");
  ok(!tab3.pinned, "Tab3 is unpinned");
  is(tab1._tPos, 0, "Tab1 should still be first after pinning");
  is(tab2._tPos, 1, "Tab2 should still be second after pinning");
  is(tab3._tPos, 2, "Tab3 should still be third after pinning");

  // Check the context menu with a multiselected and pinned tab
  updateTabContextMenu(tab2);
  ok(tab2.pinned, "Tab2 is pinned");
  is(menuItemPinTab.hidden, true, "Pin Tab is hidden");
  is(menuItemUnpinTab.hidden, true, "Unpin Tab is hidden");
  is(menuItemPinSelectedTabs.hidden, true, "Pin Selected Tabs is hidden");
  is(menuItemUnpinSelectedTabs.hidden, false, "Unpin Selected Tabs is visible");

  let tab1Unpinned = BrowserTestUtils.waitForEvent(tab1, "TabUnpinned");
  let tab2Unpinned = BrowserTestUtils.waitForEvent(tab2, "TabUnpinned");
  menuItemUnpinSelectedTabs.click();
  await tab1Unpinned;
  await tab2Unpinned;

  ok(!tab1.pinned, "Tab1 is unpinned");
  ok(!tab2.pinned, "Tab2 is unpinned");
  ok(!tab3.pinned, "Tab3 is unpinned");
  is(tab1._tPos, 0, "Tab1 should still be first after unpinning");
  is(tab2._tPos, 1, "Tab2 should still be second after unpinning");
  is(tab3._tPos, 2, "Tab3 should still be third after unpinning");

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
