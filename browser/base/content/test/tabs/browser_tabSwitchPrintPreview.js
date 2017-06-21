const kURL1 = "data:text/html,Should I stay or should I go?";
const kURL2 = "data:text/html,I shouldn't be here!";

/**
 * Verify that if we open a new tab and try to make it the selected tab while
 * print preview is up, that doesn't happen.
 * Also check that we switch back to the original tab on exiting Print Preview.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab(kURL1, async function(browser) {
    let originalTab = gBrowser.selectedTab;
    let tab = BrowserTestUtils.addTab(gBrowser, kURL2);
    document.getElementById("cmd_printPreview").doCommand();
    gBrowser.selectedTab = tab;
    await BrowserTestUtils.waitForCondition(() => gInPrintPreviewMode, "should be in print preview mode");
    isnot(gBrowser.selectedTab, tab, "Selected tab should not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should be the print preview tab");
    gBrowser.selectedTab = tab;
    isnot(gBrowser.selectedTab, tab, "Selected tab should still not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should still be the print preview tab");
    let tabSwitched = BrowserTestUtils.switchTab(gBrowser, () => { PrintUtils.exitPrintPreview(); });
    await BrowserTestUtils.waitForCondition(() => !gInPrintPreviewMode, "should no longer be in print preview mode");
    await tabSwitched;
    is(gBrowser.selectedTab, originalTab, "Selected tab should be back to the original tab that we print previewed");
    await BrowserTestUtils.removeTab(tab);
  });
});
