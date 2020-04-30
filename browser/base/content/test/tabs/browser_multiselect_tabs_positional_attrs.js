const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function checkBeforeMultiselectedAttributes() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();

  let visibleTabs = gBrowser._visibleTabs;

  await triggerClickOn(tab3, { ctrlKey: true });

  is(visibleTabs.indexOf(tab1), 1, "The index of Tab1 is one");
  is(visibleTabs.indexOf(tab2), 2, "The index of Tab2 is two");
  is(visibleTabs.indexOf(tab3), 3, "The index of Tab3 is three");

  ok(!tab1.multiselected, "Tab1 is not multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected");
  ok(tab3.multiselected, "Tab3 is multi-selected");

  ok(!tab1.beforeMultiselected, "Tab1 is not before-multiselected");
  ok(tab2.beforeMultiselected, "Tab2 is before-multiselected");

  info("Close Tab2");
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  BrowserTestUtils.removeTab(tab2);
  await tab2Closing;

  // Cache invalidated, so we need to update the collection
  visibleTabs = gBrowser._visibleTabs;

  is(visibleTabs.indexOf(tab1), 1, "The index of Tab1 is one");
  is(visibleTabs.indexOf(tab3), 2, "The index of Tab3 is two");
  ok(tab1.beforeMultiselected, "Tab1 is before-multiselected");

  // Checking if positional attributes are updated when "active" tab is clicked.
  info("Click on the active tab to clear multiselect");
  await triggerClickOn(gBrowser.selectedTab, {});

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");
  ok(!tab1.beforeMultiselected, "Tab1 is not before-multiselected anymore");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab3);
});
