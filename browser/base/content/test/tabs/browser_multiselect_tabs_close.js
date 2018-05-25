const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";
const PREF_WARN_ON_CLOSE = "browser.tabs.warnOnCloseOtherTabs";

add_task(async function setPref() {
    await SpecialPowers.pushPrefEnv({
        set: [
            [PREF_MULTISELECT_TABS, true],
            [PREF_WARN_ON_CLOSE, false]
        ]
    });
});

add_task(async function usingTabCloseButton() {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();
    let tab4 = await addTab();

    is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

    await triggerClickOn(tab1, { ctrlKey: true });
    await triggerClickOn(tab2, { ctrlKey: true });

    ok(tab1.multiselected, "Tab1 is multiselected");
    ok(tab2.multiselected, "Tab2 is multiselected");
    ok(!tab3.multiselected, "Tab3 is not multiselected");
    ok(!tab4.multiselected, "Tab4 is not multiselected");
    is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

    // Closing a tab which is not multiselected
    let tab4CloseBtn = document.getAnonymousElementByAttribute(tab4, "anonid", "close-button");
    let tab4Closing = BrowserTestUtils.waitForTabClosing(tab4);
    tab4CloseBtn.click();
    await tab4Closing;

    ok(tab1.multiselected, "Tab1 is multiselected");
    ok(!tab1.closing, "Tab1 is not closing");
    ok(tab2.multiselected, "Tab2 is multiselected");
    ok(!tab2.closing, "Tab2 is not closing");
    ok(!tab3.multiselected, "Tab3 is not multiselected");
    ok(!tab3.closing, "Tab3 is not closing");
    ok(tab4.closing, "Tab4 is closing");
    is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

    // Closing a selected tab
    let tab2CloseBtn = document.getAnonymousElementByAttribute(tab1, "anonid", "close-button");
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
    let menuItemCloseSelectedTabs = document.getElementById("context_closeSelectedTabs");

    is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

    // Check the context menu with zero multiselected tabs
    updateTabContextMenu(tab4);
    is(menuItemCloseTab.hidden, false, "Close Tab is visible");
    is(menuItemCloseSelectedTabs.hidden, true, "Close Selected Tabs is hidden");

    await triggerClickOn(tab1, { ctrlKey: true });
    await triggerClickOn(tab2, { ctrlKey: true });

    ok(tab1.multiselected, "Tab1 is multiselected");
    ok(tab2.multiselected, "Tab2 is multiselected");
    ok(!tab3.multiselected, "Tab3 is not multiselected");
    ok(!tab4.multiselected, "Tab4 is not multiselected");
    is(gBrowser.multiSelectedTabsCount, 2, "Two multiselected tabs");

    // Check the context menu with two multiselected tabs
    updateTabContextMenu(tab4);
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

