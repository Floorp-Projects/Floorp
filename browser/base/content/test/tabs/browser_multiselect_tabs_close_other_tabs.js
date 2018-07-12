const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";
const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true],
      [PREF_WARN_ON_CLOSE, false]
    ]
  });
});

add_task(async function withAMultiSelectedTab() {
  let initialTab = gBrowser.selectedTab;
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await triggerClickOn(tab1, { ctrlKey: true });

  let tab4Pinned = BrowserTestUtils.waitForEvent(tab4, "TabPinned");
  gBrowser.pinTab(tab4);
  await tab4Pinned;

  ok(initialTab.multiselected, "InitialTab is multiselected");
  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(!tab2.multiselected, "Tab2 is not multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(tab4.pinned, "Tab4 is pinned");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");
  is(gBrowser.selectedTab, initialTab, "InitialTab is the active tab");

  let closingTabs = [tab2, tab3];
  let tabClosingPromises = [];
  for (let tab of closingTabs) {
    tabClosingPromises.push(BrowserTestUtils.waitForTabClosing(tab));
  }

  gBrowser.removeAllTabsBut(tab1);

  for (let promise of tabClosingPromises) {
    await promise;
  }

  ok(!initialTab.closing, "InitialTab is not closing");
  ok(!tab1.closing, "Tab1 is not closing");
  ok(tab2.closing, "Tab2 is closing");
  ok(tab3.closing, "Tab3 is closing");
  ok(!tab4.closing, "Tab4 is not closing");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");
  is(gBrowser.selectedTab, initialTab, "InitialTab is still the active tab");

  gBrowser.clearMultiSelectedTabs(false);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab4);
});

add_task(async function withNotAMultiSelectedTab() {
  let initialTab = gBrowser.selectedTab;
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  let tab4Pinned = BrowserTestUtils.waitForEvent(tab4, "TabPinned");
  gBrowser.pinTab(tab4);
  await tab4Pinned;

  ok(!initialTab.multiselected, "InitialTab is not multiselected");
  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(tab4.pinned, "Tab4 is pinned");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");
  is(gBrowser.selectedTab, tab1, "Tab1 is the active tab");

  let closingTabs = [tab1, tab2, tab3];
  let tabClosingPromises = [];
  for (let tab of closingTabs) {
    tabClosingPromises.push(BrowserTestUtils.waitForTabClosing(tab));
  }

  await BrowserTestUtils.switchTab(gBrowser, gBrowser.removeAllTabsBut(initialTab));

  for (let promise of tabClosingPromises) {
    await promise;
  }

  ok(!initialTab.closing, "InitialTab is not closing");
  ok(tab1.closing, "Tab1 is closing");
  ok(tab2.closing, "Tab2 is closing");
  ok(tab3.closing, "Tab3 is closing");
  ok(!tab4.closing, "Tab4 is not closing");
  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");
  is(gBrowser.selectedTab, initialTab, "InitialTab is the active tab now");

  BrowserTestUtils.removeTab(tab4);
});
