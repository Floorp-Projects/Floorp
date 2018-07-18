const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]]
  });
});

add_task(async function test() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab3, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

  let newWindow = gBrowser.replaceTabsWithWindow(tab1);

  // waiting for tab2 to close ensure that the newWindow is created,
  // thus newWindow.gBrowser used in the second waitForCondition
  // will not be undefined.
  await TestUtils.waitForCondition(() => tab2.closing, "Wait for tab2 to close");
  await TestUtils.waitForCondition(() => newWindow.gBrowser.visibleTabs.length == 3,
    "Wait for all three tabs to get moved to the new window");

  let gBrowser2 = newWindow.gBrowser;

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");
  is(gBrowser.visibleTabs.length, 2, "Two tabs now in the old window");
  is(gBrowser2.visibleTabs.length, 3, "Three tabs in the new window");
  is(gBrowser2.visibleTabs.indexOf(gBrowser2.selectedTab), 1,
    "Previously active tab is still the active tab in the new window");

  BrowserTestUtils.closeWindow(newWindow);
  BrowserTestUtils.removeTab(tab4);
});
