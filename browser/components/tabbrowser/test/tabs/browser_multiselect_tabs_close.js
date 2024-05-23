const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

async function openTabMenuFor(tab) {
  let tabMenu = tab.ownerDocument.getElementById("tabContextMenu");

  let tabMenuShown = BrowserTestUtils.waitForEvent(tabMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    tab,
    { type: "contextmenu" },
    tab.ownerGlobal
  );
  await tabMenuShown;

  return tabMenu;
}

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

  await triggerClickOn(tab3, { ctrlKey: true });
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");
  gBrowser.hideTab(tab3);
  is(
    gBrowser.multiSelectedTabsCount,
    2,
    "Two multiselected tabs after hiding one tab"
  );
  gBrowser.showTab(tab3);
  is(
    gBrowser.multiSelectedTabsCount,
    3,
    "Three multiselected tabs after re-showing hidden tab"
  );
  await triggerClickOn(tab3, { ctrlKey: true });
  is(
    gBrowser.multiSelectedTabsCount,
    2,
    "Two multiselected tabs after ctrl-clicking multiselected tab"
  );

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
  let { args } = document.l10n.getAttributes(menuItemCloseTab);
  is(args.tabCount, 1, "Close Tab item lists a single tab");

  // Check the context menu with a multiselected tab. We have to actually open
  // it (not just call `updateTabContextMenu`) in order for
  // `TabContextMenu.contextTab` to stay non-null when we click an item.
  let menu = await openTabMenuFor(tab2);
  ({ args } = document.l10n.getAttributes(menuItemCloseTab));
  is(args.tabCount, 2, "Close Tab item lists more than one tab");

  let tab1Closing = BrowserTestUtils.waitForTabClosing(tab1);
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  menu.activateItem(menuItemCloseTab);
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

add_task(async function closeAllMultiselectedMiddleClick() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();
  let tab6 = await addTab();

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  // Close currently selected tab1
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  let tab1Closing = BrowserTestUtils.waitForTabClosing(tab1);
  await triggerMiddleClickOn(tab1);
  await tab1Closing;

  // Close a not currently selected tab2
  await BrowserTestUtils.switchTab(gBrowser, tab3);
  let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
  await triggerMiddleClickOn(tab2);
  await tab2Closing;

  // Close the not multiselected middle clicked tab6
  await triggerClickOn(tab4, { ctrlKey: true });
  await triggerClickOn(tab5, { ctrlKey: true });
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(tab4.multiselected, "Tab4 is multiselected");
  ok(tab5.multiselected, "Tab5 is multiselected");
  ok(!tab6.multiselected, "Tab6 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

  let tab6Closing = BrowserTestUtils.waitForTabClosing(tab6);
  await triggerMiddleClickOn(tab6);
  await tab6Closing;

  // Close multiselected tabs(3, 4, 5)
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(tab4.multiselected, "Tab4 is multiselected");
  ok(tab5.multiselected, "Tab5 is multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

  let tab3Closing = BrowserTestUtils.waitForTabClosing(tab3);
  let tab4Closing = BrowserTestUtils.waitForTabClosing(tab4);
  let tab5Closing = BrowserTestUtils.waitForTabClosing(tab5);
  await triggerMiddleClickOn(tab5);
  await tab3Closing;
  await tab4Closing;
  await tab5Closing;
});
