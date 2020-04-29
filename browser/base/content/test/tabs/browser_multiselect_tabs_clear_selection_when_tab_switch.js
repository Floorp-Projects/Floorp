add_task(async function test() {
  let initialTab = gBrowser.selectedTab;
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  for (let tab of [tab1, tab2, tab3]) {
    await triggerClickOn(tab, { ctrlKey: true });
  }

  is(gBrowser.multiSelectedTabsCount, 4, "Four multiselected tabs");
  is(gBrowser.selectedTab, initialTab, "InitialTab is the active tab");

  info("Un-select the active tab");
  await BrowserTestUtils.switchTab(
    gBrowser,
    triggerClickOn(initialTab, { ctrlKey: true })
  );

  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");
  is(gBrowser.selectedTab, tab3, "Tab3 is the active tab");

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  is(gBrowser.multiSelectedTabsCount, 0, "Selection cleared after tab-switch");
  is(gBrowser.selectedTab, tab1, "Tab1 is the active tab");

  for (let tab of [tab1, tab2, tab3]) {
    BrowserTestUtils.removeTab(tab);
  }
});
