const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

function triggerClickOn(target, options) {
  let promise = BrowserTestUtils.waitForEvent(target, "click");
  if (AppConstants.platform == "macosx") {
    options = { metaKey: options.ctrlKey };
  }
  EventUtils.synthesizeMouseAtCenter(target, options);
  return promise;
}

async function addTab() {
  const tab = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888/", {skipAnimation: true});
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

add_task(async function clickWithoutPrefSet() {
  let tab = await addTab();
  let mSelectedTabs = gBrowser._multiSelectedTabsMap;

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

add_task(async function clickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true]
    ]
  });

  let mSelectedTabs = gBrowser._multiSelectedTabsMap;
  const initialFocusedTab = gBrowser.selectedTab;
  const tab = await addTab();

  await triggerClickOn(tab, { ctrlKey: true });
  ok(tab.multiselected && mSelectedTabs.has(tab), "Tab should be (multi) selected after click");
  isnot(gBrowser.selectedTab, tab, "Multi-selected tab is not focused");
  is(gBrowser.selectedTab, initialFocusedTab, "Focused tab doesn't change");

  await triggerClickOn(tab, { ctrlKey: true });
  ok(!tab.multiselected && !mSelectedTabs.has(tab), "Tab is not selected anymore");
  is(gBrowser.selectedTab, initialFocusedTab, "Focused tab still doesn't change");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function clearSelection() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true]
    ]
  });

  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();

  info("We select tab1 and tab2 with ctrl key down");
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected && gBrowser._multiSelectedTabsMap.has(tab1), "Tab1 is (multi) selected");
  ok(tab2.multiselected && gBrowser._multiSelectedTabsMap.has(tab2), "Tab2 is (multi) selected");
  is(gBrowser.multiSelectedTabsCount(), 2, "Two tabs selected");
  isnot(tab3, gBrowser.selectedTab, "Tab3 doesn't have focus");

  info("We select tab3 with Ctrl key up");
  await triggerClickOn(tab3, { ctrlKey: false });

  ok(!tab1.multiselected, "Tab1 is unselected");
  ok(!tab2.multiselected, "Tab2 is unselected");
  is(gBrowser.multiSelectedTabsCount(), 0, "Selection is cleared");
  is(tab3, gBrowser.selectedTab, "Tab3 has focus");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
