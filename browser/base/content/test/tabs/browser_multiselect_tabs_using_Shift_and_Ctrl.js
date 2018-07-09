const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true]
    ]
  });
});

add_task(async function selectionWithShiftPreviously() {
  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();
  const tab4 = await addTab();
  const tab5 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, tab3);

  is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

  info("Click on tab5 with Shift down");
  await triggerClickOn(tab5, { shiftKey: true });

  is(gBrowser.selectedTab, tab3, "Tab3 has focus");
  ok(!tab1.multiselected, "Tab1 is not multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected ");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  ok(tab4.multiselected, "Tab4 is multi-selected");
  ok(tab5.multiselected, "Tab5 is multi-selected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three tabs are multi-selected");

  info("Click on tab1 with both Ctrl/Cmd and Shift down");
  await triggerClickOn(tab1, { ctrlKey: true, shiftKey: true });

  is(gBrowser.selectedTab, tab3, "Tab3 has focus");
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected ");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  ok(tab4.multiselected, "Tab4 is multi-selected");
  ok(tab5.multiselected, "Tab5 is multi-selected");
  is(gBrowser.multiSelectedTabsCount, 5, "Five tabs are multi-selected");

  for (let tab of [tab1, tab2, tab3, tab4, tab5])
    BrowserTestUtils.removeTab(tab);
});

add_task(async function selectionWithCtrlPreviously() {
  const tab1 = await addTab();
  const tab2 = await addTab();
  const tab3 = await addTab();
  const tab4 = await addTab();
  const tab5 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

  info("Click on tab3 with Ctrl key down");
  await triggerClickOn(tab5, { ctrlKey: true });

  is(gBrowser.selectedTab, tab1, "Tab1 has focus");
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected ");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  ok(!tab4.multiselected, "Tab4 is not multi-selected");
  ok(!tab5.multiselected, "Tab5 is not multi-selected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two tabs are multi-selected");

  info("Click on tab5 with both Ctrl/Cmd and Shift down");
  await triggerClickOn(tab5, { ctrlKey: true, shiftKey: true });

  is(gBrowser.selectedTab, tab1, "Tab3 has focus");
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected ");
  ok(tab3.multiselected, "Tab3 is multi-selected");
  ok(tab4.multiselected, "Tab4 is multi-selected");
  ok(tab5.multiselected, "Tab5 is multi-selected");
  is(gBrowser.multiSelectedTabsCount, 4, "Four tabs are multi-selected");

  for (let tab of [tab1, tab2, tab3, tab4, tab5])
    BrowserTestUtils.removeTab(tab);
});
