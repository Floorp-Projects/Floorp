const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function withAMultiSelectedTab() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab3, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(!tab2.multiselected, "Tab2 is not multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(!tab5.multiselected, "Tab5 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  // Tab2 will be closed because tab1 is the contextTab.
  let closingTabs = [tab2, tab4, tab5];
  let tabClosingPromises = [];
  for (let tab of closingTabs) {
    tabClosingPromises.push(BrowserTestUtils.waitForTabClosing(tab));
  }

  gBrowser.removeTabsToTheEndFrom(tab1);

  for (let promise of tabClosingPromises) {
    await promise;
  }

  ok(!tab1.closing, "Tab1 is not closing");
  ok(tab2.closing, "Tab2 is closing");
  ok(!tab3.closing, "Tab3 is not closing");
  ok(tab4.closing, "Tab4 is closing");
  ok(tab5.closing, "Tab5 is closing");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  for (let tab of [tab1, tab2, tab3]) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function withNotAMultiSelectedTab() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab3, { ctrlKey: true });
  await triggerClickOn(tab5, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(!tab2.multiselected, "Tab2 is not multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(tab5.multiselected, "Tab5 is multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

  let closingTabs = [tab5];
  let tabClosingPromises = [];
  for (let tab of closingTabs) {
    tabClosingPromises.push(BrowserTestUtils.waitForTabClosing(tab));
  }

  gBrowser.removeTabsToTheEndFrom(tab4);

  for (let promise of tabClosingPromises) {
    await promise;
  }

  ok(!tab1.closing, "Tab1 is not closing");
  ok(!tab2.closing, "Tab2 is not closing");
  ok(!tab3.closing, "Tab3 is not closing");
  ok(!tab4.closing, "Tab4 is not closing");
  ok(tab5.closing, "Tab5 is closing");
  is(gBrowser.multiSelectedTabsCount, 2, "Selection is not cleared");

  closingTabs = [tab3, tab4];
  tabClosingPromises = [];
  for (let tab of closingTabs) {
    tabClosingPromises.push(BrowserTestUtils.waitForTabClosing(tab));
  }

  gBrowser.removeTabsToTheEndFrom(tab2);

  for (let promise of tabClosingPromises) {
    await promise;
  }

  ok(!tab1.closing, "Tab1 is not closing");
  ok(!tab2.closing, "Tab2 is not closing");
  ok(tab3.closing, "Tab3 is closing");
  ok(tab4.closing, "Tab4 is closing");
  is(gBrowser.multiSelectedTabsCount, 0, "Selection is cleared");

  for (let tab of [tab1, tab2]) {
    BrowserTestUtils.removeTab(tab);
  }
});
