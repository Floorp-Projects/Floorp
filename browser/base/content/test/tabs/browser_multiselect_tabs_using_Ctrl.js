const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function clickWithoutPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, false]]
  });

  let tab = await addTab();
  let mSelectedTabs = gBrowser._multiSelectedTabsSet;

  isnot(gBrowser.selectedTab, tab, "Tab doesn't have focus");

  // We make sure that the tab-switch is completely done before executing checks
  await BrowserTestUtils.switchTab(gBrowser, () => {
    triggerClickOn(tab, { ctrlKey: true });
  });

  await TestUtils.waitForCondition(() => gBrowser.selectedTab == tab,
    "Wait for the selectedTab getter to update");

  ok(!tab.multiselected && !mSelectedTabs.has(tab),
    "Multi-select tab doesn't work when multi-select pref is not set");
  is(gBrowser.selectedTab, tab,
    "Tab has focus, selected tab has changed after Ctrl/Cmd + click");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
      set: [[PREF_MULTISELECT_TABS, true]]
  });
});

add_task(async function clickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]]
  });

  let mSelectedTabs = gBrowser._multiSelectedTabsSet;
  const initialFocusedTab = await addTab();
  await BrowserTestUtils.switchTab(gBrowser, initialFocusedTab);
  const tab = await addTab();

  await triggerClickOn(tab, { ctrlKey: true });
  ok(tab.multiselected && mSelectedTabs.has(tab), "Tab should be (multi) selected after click");
  isnot(gBrowser.selectedTab, tab, "Multi-selected tab is not focused");
  is(gBrowser.selectedTab, initialFocusedTab, "Focused tab doesn't change");

  await triggerClickOn(tab, { ctrlKey: true });
  ok(!tab.multiselected && !mSelectedTabs.has(tab), "Tab is not (multi) selected anymore");
  is(gBrowser.selectedTab, initialFocusedTab, "Focused tab still doesn't change");

  BrowserTestUtils.removeTab(initialFocusedTab);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function clearSelection() {
  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  info("We multi-select tab2 with ctrl key down");
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected && gBrowser._multiSelectedTabsSet.has(tab1), "Tab1 is (multi) selected");
  ok(tab2.multiselected && gBrowser._multiSelectedTabsSet.has(tab2), "Tab2 is (multi) selected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two tabs (multi) selected");
  isnot(tab3, gBrowser.selectedTab, "Tab3 doesn't have focus");

  info("We select tab3 with Ctrl key up");
  await triggerClickOn(tab3, { ctrlKey: false });

  ok(!tab1.multiselected, "Tab1 is not (multi) selected");
  ok(!tab2.multiselected, "Tab2 is not (multi) selected");
  is(gBrowser.multiSelectedTabsCount, 0, "Multi-selection is cleared");
  is(tab3, gBrowser.selectedTab, "Tab3 has focus");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
