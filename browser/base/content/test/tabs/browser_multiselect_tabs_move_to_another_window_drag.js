add_task(async function test() {
  // Disable tab animations
  gReduceMotionOverride = true;

  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab("http://mochi.test:8888/3");
  let tab4 = await addTab();
  let tab5 = await addTab("http://mochi.test:8888/5");

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab1, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(!tab5.multiselected, "Tab5 is not multiselected");
  is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

  let newWindow = gBrowser.replaceTabsWithWindow(tab1);

  // waiting for tab2 to close ensure that the newWindow is created,
  // thus newWindow.gBrowser used in the second waitForCondition
  // will not be undefined.
  await TestUtils.waitForCondition(
    () => tab2.closing,
    "Wait for tab2 to close"
  );
  await TestUtils.waitForCondition(
    () => newWindow.gBrowser.visibleTabs.length == 2,
    "Wait for all two tabs to get moved to the new window"
  );

  let gBrowser2 = newWindow.gBrowser;
  tab1 = gBrowser2.visibleTabs[0];
  tab2 = gBrowser2.visibleTabs[1];

  if (gBrowser.selectedTab != tab3) {
    await BrowserTestUtils.switchTab(gBrowser, tab3);
  }

  await triggerClickOn(tab5, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(tab3.multiselected, "Tab3 is multiselected");
  ok(!tab4.multiselected, "Tab4 is not multiselected");
  ok(tab5.multiselected, "Tab5 is multiselected");

  await dragAndDrop(tab3, tab1, false, newWindow);

  await TestUtils.waitForCondition(
    () => gBrowser2.visibleTabs.length == 4,
    "Moved tab3 and tab5 to second window"
  );

  tab3 = gBrowser2.visibleTabs[1];
  tab5 = gBrowser2.visibleTabs[2];

  await BrowserTestUtils.waitForCondition(
    () => getUrl(tab3) == "http://mochi.test:8888/3"
  );
  await BrowserTestUtils.waitForCondition(
    () => getUrl(tab5) == "http://mochi.test:8888/5"
  );

  ok(true, "Tab3 and tab5 are duplicated succesfully");

  BrowserTestUtils.closeWindow(newWindow);
  BrowserTestUtils.removeTab(tab4);
});
