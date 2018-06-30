const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function prefNotSet() {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();

    let mSelectedTabs = gBrowser._multiSelectedTabsSet;

    await BrowserTestUtils.switchTab(gBrowser, tab1);

    is(gBrowser.selectedTab, tab1, "Tab1 has focus now");
    is(gBrowser.multiSelectedTabsCount, 0, "No tab is mutli-selected");

    info("Click on tab3 while holding shift key");
    await BrowserTestUtils.switchTab(gBrowser, () => {
        triggerClickOn(tab3, { shiftKey: true });
    });

    ok(!tab1.multiselected && !mSelectedTabs.has(tab1), "Tab1 is not multi-selected");
    ok(!tab2.multiselected && !mSelectedTabs.has(tab2), "Tab2 is not multi-selected");
    ok(!tab3.multiselected && !mSelectedTabs.has(tab3), "Tab3 is not multi-selected");
    is(gBrowser.multiSelectedTabsCount, 0, "There is still no multi-selected tab");
    is(gBrowser.selectedTab, tab3, "Tab3 has focus now");

    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
    BrowserTestUtils.removeTab(tab3);
});

add_task(async function setPref() {
    await SpecialPowers.pushPrefEnv({
        set: [
            [PREF_MULTISELECT_TABS, true]
        ]
    });
});

add_task(async function noItemsInTheCollectionBeforeShiftClicking() {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();
    let tab4 = await addTab();
    let tab5 = await addTab();
    let mSelectedTabs = gBrowser._multiSelectedTabsSet;

    await BrowserTestUtils.switchTab(gBrowser, tab1);

    is(gBrowser.selectedTab, tab1, "Tab1 has focus now");
    is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

    gBrowser.hideTab(tab3);
    ok(tab3.hidden, "Tab3 is hidden");

    info("Click on tab4 while holding shift key");
    await triggerClickOn(tab4, { shiftKey: true });
    mSelectedTabs = gBrowser._multiSelectedTabsSet;

    ok(tab1.multiselected && mSelectedTabs.has(tab1), "Tab1 is multi-selected");
    ok(tab2.multiselected && mSelectedTabs.has(tab2), "Tab2 is multi-selected");
    ok(!tab3.multiselected && !mSelectedTabs.has(tab3), "Hidden tab3 is not multi-selected");
    ok(tab4.multiselected && mSelectedTabs.has(tab4), "Tab4 is multi-selected");
    ok(!tab5.multiselected && !mSelectedTabs.has(tab5), "Tab5 is not multi-selected");
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

    let mSelectedTabs = gBrowser._multiSelectedTabsSet;

    await BrowserTestUtils.switchTab(gBrowser, () => triggerClickOn(tab1, {}));

    is(gBrowser.selectedTab, tab1, "Tab1 has focus now");
    is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

    await triggerClickOn(tab3, { ctrlKey: true });
    is(gBrowser.selectedTab, tab1, "Tab1 still has focus");
    is(gBrowser.multiSelectedTabsCount, 2, "Two tabs are multi-selected");
    ok(tab1.multiselected && mSelectedTabs.has(tab1), "Tab1 is multi-selected");
    ok(tab3.multiselected && mSelectedTabs.has(tab3), "Tab3 is multi-selected");

    info("Click on tab5 while holding Shift key");
    await triggerClickOn(tab5, { shiftKey: true });
    mSelectedTabs = gBrowser._multiSelectedTabsSet;

    is(gBrowser.selectedTab, tab3, "Tab3 has focus");
    ok(!tab1.multiselected && !mSelectedTabs.has(tab1), "Tab1 is not multi-selected");
    ok(!tab2.multiselected && !mSelectedTabs.has(tab2), "Tab2 is not multi-selected ");
    ok(tab3.multiselected && mSelectedTabs.has(tab3), "Tab3 is multi-selected");
    ok(tab4.multiselected && mSelectedTabs.has(tab4), "Tab4 is multi-selected");
    ok(tab5.multiselected && mSelectedTabs.has(tab5), "Tab5 is multi-selected");
    is(gBrowser.multiSelectedTabsCount, 3, "Three tabs are multi-selected");

    info("Click on tab1 while holding Shift key");
    await triggerClickOn(tab1, { shiftKey: true });
    mSelectedTabs = gBrowser._multiSelectedTabsSet;

    is(gBrowser.selectedTab, tab3, "Tab3 has focus");
    ok(tab1.multiselected && mSelectedTabs.has(tab1), "Tab1 is multi-selected");
    ok(tab2.multiselected && mSelectedTabs.has(tab2), "Tab2 is multi-selected ");
    ok(tab3.multiselected && mSelectedTabs.has(tab3), "Tab3 is multi-selected");
    ok(!tab4.multiselected && !mSelectedTabs.has(tab4), "Tab4 is not multi-selected");
    ok(!tab5.multiselected && !mSelectedTabs.has(tab5), "Tab5 is not multi-selected");
    is(gBrowser.multiSelectedTabsCount, 3, "Three tabs are multi-selected");

    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
    BrowserTestUtils.removeTab(tab3);
    BrowserTestUtils.removeTab(tab4);
    BrowserTestUtils.removeTab(tab5);
});

add_task(async function shiftHasHigherPrecOverCtrl() {
    const tab1 = await addTab();
    const tab2 = await addTab();

    await BrowserTestUtils.switchTab(gBrowser, tab1);

    is(gBrowser.multiSelectedTabsCount, 0, "No tab is multi-selected");

    info("Click on tab2 with both Ctrl/Cmd and Shift down");
    await triggerClickOn(tab2, { ctrlKey: true, shiftKey: true });

    is(gBrowser.multiSelectedTabsCount, 2, "Both tab1 and tab2 are multi-selected");

    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
});
