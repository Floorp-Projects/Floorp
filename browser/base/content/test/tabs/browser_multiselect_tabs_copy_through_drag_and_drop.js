add_task(async function test() {
  let tab0 = gBrowser.selectedTab;
  let tab1 = await addTab("http://example.com/1");
  let tab2 = await addTab("http://example.com/2");
  let tab3 = await addTab("http://example.com/3");
  let tabs = [tab0, tab1, tab2, tab3];

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  is(gBrowser.selectedTab, tab1, "Tab1 is active");
  is(gBrowser.selectedTabs.length, 2, "Two selected tabs");
  is(gBrowser.visibleTabs.length, 4, "Four tabs in window before copy");

  for (let i of [1, 2]) {
    ok(tabs[i].multiselected, "Tab" + i + " is multiselected");
  }
  for (let i of [0, 3]) {
    ok(!tabs[i].multiselected, "Tab" + i + " is not multiselected");
  }

  await dragAndDrop(tab1, tab3, true);

  is(gBrowser.selectedTab, tab1, "tab1 is still active");
  is(gBrowser.selectedTabs.length, 2, "Two selected tabs");
  is(gBrowser.visibleTabs.length, 6, "Six tabs in window after copy");

  let tab4 = gBrowser.visibleTabs[4];
  let tab5 = gBrowser.visibleTabs[5];
  tabs.push(tab4);
  tabs.push(tab5);

  for (let i of [1, 2]) {
    ok(tabs[i].multiselected, "Tab" + i + " is multiselected");
  }
  for (let i of [0, 3, 4, 5]) {
    ok(!tabs[i].multiselected, "Tab" + i + " is not multiselected");
  }

  await BrowserTestUtils.waitForCondition(() => getUrl(tab4) == getUrl(tab1));
  await BrowserTestUtils.waitForCondition(() => getUrl(tab5) == getUrl(tab2));

  ok(true, "Tab1 and tab2 are duplicated succesfully");

  for (let tab of tabs.filter(t => t != tab0)) {
    BrowserTestUtils.removeTab(tab);
  }
});
