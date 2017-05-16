const kURL1 = "data:text/html,Should I stay or should I go?";
const kURL2 = "data:text/html,I shouldn't be here!";

/**
 * Verify that if we open a new tab and try to make it the selected tab while
 * print preview is up, that doesn't happen.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab(kURL1, async function(browser) {
    let tab = BrowserTestUtils.addTab(gBrowser, kURL2);
    document.getElementById("cmd_printPreview").doCommand();
    gBrowser.selectedTab = tab;
    await BrowserTestUtils.waitForCondition(() => gInPrintPreviewMode, "should be in print preview mode");
    isnot(gBrowser.selectedTab, tab, "Selected tab should not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should be the print preview tab");
    gBrowser.selectedTab = tab;
    isnot(gBrowser.selectedTab, tab, "Selected tab should still not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should still be the print preview tab");
    PrintUtils.exitPrintPreview();
    await BrowserTestUtils.waitForCondition(() => !gInPrintPreviewMode, "should be in print preview mode");
    await BrowserTestUtils.removeTab(tab);
  });
});
