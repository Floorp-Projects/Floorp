const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function usingTabCloseButton() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");
  is(gBrowser.selectedTab, tab1, "Tab1 is active");

  // Closing a tab which is not multiselected
  let tab4CloseBtn = tab4.closeButton;
  let tab4Closing = BrowserTestUtils.waitForTabClosing(tab4);

  tab4.mOverCloseButton = true;
  ok(tab4.mOverCloseButton, "Mouse over tab4 close button");
  tab4CloseBtn.click();
  await tab4Closing;

  is(gBrowser.selectedTab, tab1, "Tab1 is active");
  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(!tab1.closing, "Tab1 is not closing");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab2.closing, "Tab2 is not closing");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab3.closing, "Tab3 is not closing");
  ok(tab4.closing, "Tab4 is closing");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  // Closing a selected tab
  let tab2CloseBtn = tab2.closeButton;
  tab2.mOverCloseButton = true;
  let tab1Closing = BrowserTestUtils.waitForTabClosing(tab1);
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  tab2CloseBtn.click();
  await tab1Closing;
  await tab2Closing;

  ok(tab1.closing, "Tab1 is closing");
  ok(tab2.closing, "Tab2 is closing");
  ok(!tab3.closing, "Tab3 is not closing");
  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  BrowserTestUtils.removeTab(tab3);
});

add_task(async function usingTabContextMenu() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();

  let menuItemCloseTab = document.getElementById("context_closeTab");
  let menuItemCloseSelectedTabs = document.getElementById(
    "context_closeSelectedTabs"
  );

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  // Check the context menu with a non-multiselected tab
  updateTabContextMenu(tab4);
  is(menuItemCloseTab.hidden, false, "Close Tab is visible");
  is(menuItemCloseSelectedTabs.hidden, true, "Close Selected Tabs is hidden");

  // Check the context menu with a multiselected tab
  updateTabContextMenu(tab2);
  is(menuItemCloseTab.hidden, true, "Close Tab is hidden");
  is(menuItemCloseSelectedTabs.hidden, false, "Close Selected Tabs is visible");

  let tab1Closing = BrowserTestUtils.waitForTabClosing(tab1);
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  menuItemCloseSelectedTabs.click();
  await tab1Closing;
  await tab2Closing;

  ok(tab1.closing, "Tab1 is closing");
  ok(tab2.closing, "Tab2 is closing");
  ok(!tab3.closing, "Tab3 is not closing");
  ok(!tab4.closing, "Tab4 is not closing");
  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});
