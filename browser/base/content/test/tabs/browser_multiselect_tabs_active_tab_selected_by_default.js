const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function multiselectActiveTabByDefault() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]]
  });

  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  info("Try multiselecting Tab1 (active) with click+CtrlKey");
  await triggerClickOn(tab1, { ctrlKey: true });

  is(gBrowser.selectedTab, tab1, "Tab1 is active");
  ok(!tab1.multiselected,
    "Tab1 is not multi-selected because we are not in multi-select context yet");
  ok(!tab2.multiselected, "Tab2 is not multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");
  is(gBrowser.multiSelectedTabsCount, 0, "Zero tabs multi-selected");

  info("We multi-select tab1 and tab2 with ctrl key down");
  await triggerClickOn(tab2, { ctrlKey: true });
  await triggerClickOn(tab3, { ctrlKey: true });

  is(gBrowser.selectedTab, tab1, "Tab1 is active");
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three tabs multi-selected");
  is(gBrowser.lastMultiSelectedTab, tab3, "Tab3 is the last multi-selected tab");

  info("Unselect tab1 from multi-selection using ctrlKey");

  await BrowserTestUtils.switchTab(gBrowser, triggerClickOn(tab1, { ctrlKey: true }));

  isnot(gBrowser.selectedTab, tab1, "Tab1 is not active anymore");
  is(gBrowser.selectedTab, tab3, "Tab3 is active");
  ok(!tab1.multiselected, "Tab1 is not multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two tabs multi-selected");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
