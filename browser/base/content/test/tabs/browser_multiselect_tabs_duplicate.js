const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function setPref() {
    await SpecialPowers.pushPrefEnv({
        set: [[PREF_MULTISELECT_TABS, true]]
    });
});

add_task(async function test() {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();

    let menuItemDuplicateTab = document.getElementById("context_duplicateTab");

    is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

    await BrowserTestUtils.switchTab(gBrowser, tab1);
    await triggerClickOn(tab2, { ctrlKey: true });

    ok(tab1.multiselected, "Tab1 is multiselected");
    ok(tab2.multiselected, "Tab2 is multiselected");
    ok(!tab3.multiselected, "Tab3 is not multiselected");

    // Check the context menu with a multiselected tabs
    updateTabContextMenu(tab2);
    is(menuItemDuplicateTab.hidden, true, "Duplicate Tab is hidden");

    // Check the context menu with a non-multiselected tab
    updateTabContextMenu(tab3);
    is(menuItemDuplicateTab.hidden, false, "Duplicate Tab is visible");

    let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, "http://mochi.test:8888/");
    window.TabContextMenu.contextTab = tab3; // Set proper context for command handler
    menuItemDuplicateTab.click();
    let tab4 = await newTabOpened;

    // Selection should be cleared after duplication
    ok(!tab1.multiselected, "Tab1 is not multiselected");
    ok(!tab2.multiselected, "Tab2 is not multiselected");
    ok(!tab4.multiselected, "Tab3 is not multiselected");
    ok(!tab3.multiselected, "Tab4 is not multiselected");

    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
    BrowserTestUtils.removeTab(tab3);
    BrowserTestUtils.removeTab(tab4);
});
