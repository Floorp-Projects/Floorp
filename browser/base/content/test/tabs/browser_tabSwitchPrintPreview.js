const kURL1 = "data:text/html,Should I stay or should I go?";
const kURL2 = "data:text/html,I shouldn't be here!";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]]
  });
});

/**
 * Verify that if we open a new tab and try to make it the selected tab while
 * print preview is up, that doesn't happen.
 */
add_task(function* () {
  yield BrowserTestUtils.withNewTab(kURL1, function* (browser) {
    let tab = gBrowser.addTab(kURL2);
    document.getElementById("cmd_printPreview").doCommand();
    gBrowser.selectedTab = tab;
    yield BrowserTestUtils.waitForCondition(() => gInPrintPreviewMode, "should be in print preview mode");
    isnot(gBrowser.selectedTab, tab, "Selected tab should not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should be the print preview tab");
    gBrowser.selectedTab = tab;
    isnot(gBrowser.selectedTab, tab, "Selected tab should still not be the tab we added");
    is(gBrowser.selectedTab, PrintPreviewListener._printPreviewTab, "Selected tab should still be the print preview tab");
    PrintUtils.exitPrintPreview();
    yield BrowserTestUtils.waitForCondition(() => !gInPrintPreviewMode, "should be in print preview mode");
    yield BrowserTestUtils.removeTab(tab);
  });
});
