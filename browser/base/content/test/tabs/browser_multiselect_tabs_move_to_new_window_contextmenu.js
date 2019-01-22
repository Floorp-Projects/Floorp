const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]],
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

add_task(async function testLazyTabs() {
  let params = {createLazyBrowser: true};
  let tabs = [];
  let numTabs = 4;
  for (let i = 0; i < numTabs; ++i) {
    tabs.push(BrowserTestUtils.addTab(gBrowser, `http://example.com/?${i}`, params));
  }

  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  for (let i = 1; i < numTabs; ++i) {
    await triggerClickOn(tabs[i], { ctrlKey: true });
  }

  isnot(tabs[0].linkedPanel, "", `Tab 0 shouldn't be lazy`);
  for (let i = 1; i < numTabs; ++i) {
    is(tabs[i].linkedPanel, "", `Tab ${i} should be lazy`);
  }

  is(gBrowser.multiSelectedTabsCount, numTabs, `${numTabs} multiselected tabs`);
  for (let i = 0; i < numTabs; ++i) {
    ok(tabs[i].multiselected, `Tab ${i} should be multiselected`);
  }

  let newWindow = gBrowser.replaceTabsWithWindow(tabs[0]);

  await TestUtils.waitForCondition(() => newWindow.gBrowser, `Wait for gBrowser`);
  await TestUtils.waitForCondition(() => newWindow.gBrowser.visibleTabs.length == numTabs,
    `Wait for all ${numTabs} tabs to get moved to the new window`);
  tabs = newWindow.gBrowser.tabs;

  isnot(tabs[0].linkedPanel, "", `Tab 0 should continue not being lazy`);
  // FIXME: bug 1521923 - First inactive tab to be moved into another window loses laziness
  todo_is(tabs[1].linkedPanel, "", `Tab 1 should continue being lazy`);
  for (let i = 2; i < numTabs; ++i) {
    is(tabs[i].linkedPanel, "", `Tab ${i} should continue being lazy`);
  }

  is(tabs[0].linkedBrowser.currentURI.spec, `http://example.com/?0`,
    `Tab 0 should have the right URL`);
  todo_is(SessionStore.getLazyTabValue(tabs[1], "url"), `http://example.com/?1`,
    `Tab 1 should have the right lazy URL`);
  for (let i = 2; i < numTabs; ++i) {
    is(SessionStore.getLazyTabValue(tabs[i], "url"), `http://example.com/?${i}`,
      `Tab ${i} should have the right lazy URL`);
  }

  BrowserTestUtils.closeWindow(newWindow);
});
