add_task(async function noItemsInTheCollectionBeforeShiftClicking() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  is(gBrowser.selectedTab, tab1, "Tab1 has focus now");
  is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

  gBrowser.hideTab(tab3);
  ok(tab3.hidden, "Tab3 is hidden");

  info("Click on tab4 while holding shift key");
  await triggerClickOn(tab4, { shiftKey: true });

  ok(
    tab1.multiselected && gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is multi-selected"
  );
  ok(
    tab2.multiselected && gBrowser._multiSelectedTabsSet.has(tab2),
    "Tab2 is multi-selected"
  );
  ok(
    !tab3.multiselected && !gBrowser._multiSelectedTabsSet.has(tab3),
    "Hidden tab3 is not multi-selected"
  );
  ok(
    tab4.multiselected && gBrowser._multiSelectedTabsSet.has(tab4),
    "Tab4 is multi-selected"
  );
  ok(
    !tab5.multiselected && !gBrowser._multiSelectedTabsSet.has(tab5),
    "Tab5 is not multi-selected"
  );
  is(gBrowser.multiSelectedTabsCount, 3, "three multi-selected tabs");
  is(gBrowser.selectedTab, tab1, "Tab1 still has focus");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
  BrowserTestUtils.removeTab(tab5);
});

add_task(async function itemsInTheCollectionBeforeShiftClicking() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();
  let tab4 = await addTab();
  let tab5 = await addTab();

  await BrowserTestUtils.switchTab(gBrowser, () => triggerClickOn(tab1, {}));

  is(gBrowser.selectedTab, tab1, "Tab1 has focus now");
  is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

  await triggerClickOn(tab3, { ctrlKey: true });
  is(gBrowser.selectedTab, tab1, "Tab1 still has focus");
  is(gBrowser.multiSelectedTabsCount, 2, "Two tabs are multi-selected");
  ok(
    tab1.multiselected && gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is multi-selected"
  );
  ok(
    tab3.multiselected && gBrowser._multiSelectedTabsSet.has(tab3),
    "Tab3 is multi-selected"
  );

  info("Click on tab5 while holding Shift key");
  await BrowserTestUtils.switchTab(
    gBrowser,
    triggerClickOn(tab5, { shiftKey: true })
  );

  is(gBrowser.selectedTab, tab3, "Tab3 has focus");
  ok(
    !tab1.multiselected && !gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is not multi-selected"
  );
  ok(
    !tab2.multiselected && !gBrowser._multiSelectedTabsSet.has(tab2),
    "Tab2 is not multi-selected "
  );
  ok(
    tab3.multiselected && gBrowser._multiSelectedTabsSet.has(tab3),
    "Tab3 is multi-selected"
  );
  ok(
    tab4.multiselected && gBrowser._multiSelectedTabsSet.has(tab4),
    "Tab4 is multi-selected"
  );
  ok(
    tab5.multiselected && gBrowser._multiSelectedTabsSet.has(tab5),
    "Tab5 is multi-selected"
  );
  is(gBrowser.multiSelectedTabsCount, 3, "Three tabs are multi-selected");

  info("Click on tab4 while holding Shift key");
  await triggerClickOn(tab4, { shiftKey: true });

  is(gBrowser.selectedTab, tab3, "Tab3 has focus");
  ok(
    !tab1.multiselected && !gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is not multi-selected"
  );
  ok(
    !tab2.multiselected && !gBrowser._multiSelectedTabsSet.has(tab2),
    "Tab2 is not multi-selected "
  );
  ok(
    tab3.multiselected && gBrowser._multiSelectedTabsSet.has(tab3),
    "Tab3 is multi-selected"
  );
  ok(
    tab4.multiselected && gBrowser._multiSelectedTabsSet.has(tab4),
    "Tab4 is multi-selected"
  );
  ok(
    !tab5.multiselected && !gBrowser._multiSelectedTabsSet.has(tab5),
    "Tab5 is not multi-selected"
  );
  is(gBrowser.multiSelectedTabsCount, 2, "Two tabs are multi-selected");

  info("Click on tab1 while holding Shift key");
  await triggerClickOn(tab1, { shiftKey: true });

  is(gBrowser.selectedTab, tab3, "Tab3 has focus");
  ok(
    tab1.multiselected && gBrowser._multiSelectedTabsSet.has(tab1),
    "Tab1 is multi-selected"
  );
  ok(
    tab2.multiselected && gBrowser._multiSelectedTabsSet.has(tab2),
    "Tab2 is multi-selected "
  );
  ok(
    tab3.multiselected && gBrowser._multiSelectedTabsSet.has(tab3),
    "Tab3 is multi-selected"
  );
  ok(
    !tab4.multiselected && !gBrowser._multiSelectedTabsSet.has(tab4),
    "Tab4 is not multi-selected"
  );
  ok(
    !tab5.multiselected && !gBrowser._multiSelectedTabsSet.has(tab5),
    "Tab5 is not multi-selected"
  );
  is(gBrowser.multiSelectedTabsCount, 3, "Three tabs are multi-selected");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
  BrowserTestUtils.removeTab(tab5);
});
