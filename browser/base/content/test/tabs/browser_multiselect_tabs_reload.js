const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

async function tabLoaded(tab) {
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);
    return true;
}

add_task(async function setPref() {
    await SpecialPowers.pushPrefEnv({
        set: [[PREF_MULTISELECT_TABS, true]]
    });
});

add_task(async function test() {
    let tab1 = await addTab();
    let tab2 = await addTab();
    let tab3 = await addTab();

    let menuItemReloadTab = document.getElementById("context_reloadTab");
    let menuItemReloadSelectedTabs = document.getElementById("context_reloadSelectedTabs");

    await BrowserTestUtils.switchTab(gBrowser, tab1);
    await triggerClickOn(tab2, { ctrlKey: true });

    ok(tab1.multiselected, "Tab1 is multi-selected");
    ok(tab2.multiselected, "Tab2 is multi-selected");
    ok(!tab3.multiselected, "Tab3 is not multi-selected");

    updateTabContextMenu(tab3);
    is(menuItemReloadTab.hidden, false, "Reload Tab is visible");
    is(menuItemReloadSelectedTabs.hidden, true, "Reload Selected Tabs is hidden");

    updateTabContextMenu(tab2);
    is(menuItemReloadTab.hidden, true, "Reload Tab is hidden");
    is(menuItemReloadSelectedTabs.hidden, false, "Reload Selected Tabs is visible");

    let tab1Loaded = tabLoaded(tab1);
    let tab2Loaded = tabLoaded(tab2);
    menuItemReloadSelectedTabs.click();
    await tab1Loaded;
    await tab2Loaded;

    // We got here because tab1 and tab2 are reloaded. Otherwise the test would have timed out and failed.
    ok(true, "Tab1 and Tab2 are reloaded");

    BrowserTestUtils.removeTab(tab1);
    BrowserTestUtils.removeTab(tab2);
    BrowserTestUtils.removeTab(tab3);
});
