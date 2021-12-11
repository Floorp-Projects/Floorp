const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_WARN_ON_CLOSE, false]],
  });
});

add_task(async function using_Ctrl_W() {
  for (let key of ["w", "VK_F4"]) {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();
    let tab4 = await addTab();

    await BrowserTestUtils.switchTab(gBrowser, triggerClickOn(tab1, {}));

    is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

    await triggerClickOn(tab2, { ctrlKey: true });
    await triggerClickOn(tab3, { ctrlKey: true });

    ok(tab1.multiselected, "Tab1 is multiselected");
    ok(tab2.multiselected, "Tab2 is multiselected");
    ok(tab3.multiselected, "Tab3 is multiselected");
    ok(!tab4.multiselected, "Tab4 is not multiselected");
    is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

    let tab1Closing = BrowserTestUtils.waitForTabClosing(tab1);
    let tab2Closing = BrowserTestUtils.waitForTabClosing(tab2);
    let tab3Closing = BrowserTestUtils.waitForTabClosing(tab3);

    EventUtils.synthesizeKey(key, { accelKey: true });

    // On OSX, Cmd+F4 should not close tabs.
    const shouldBeClosing = key == "w" || AppConstants.platform != "macosx";

    if (shouldBeClosing) {
      await tab1Closing;
      await tab2Closing;
      await tab3Closing;
    }

    ok(!tab4.closing, "Tab4 is not closing");

    if (shouldBeClosing) {
      ok(tab1.closing, "Tab1 is closing");
      ok(tab2.closing, "Tab2 is closing");
      ok(tab3.closing, "Tab3 is closing");
      is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");
    } else {
      ok(!tab1.closing, "Tab1 is not closing");
      ok(!tab2.closing, "Tab2 is not closing");
      ok(!tab3.closing, "Tab3 is not closing");
      is(gBrowser.multiSelectedTabsCount, 3, "Still Three multiselected tabs");

      BrowserTestUtils.removeTab(tab1);
      BrowserTestUtils.removeTab(tab2);
      BrowserTestUtils.removeTab(tab3);
    }

    BrowserTestUtils.removeTab(tab4);
  }
});
