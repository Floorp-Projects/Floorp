add_task(function* () {
  const html = "<p id=\"p1\" title=\"tooltip is here\">This paragraph has a tooltip.</p>";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html," + html);

  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["ui.tooltipDelay", 0]]}, resolve);
  });

  yield BrowserTestUtils.synthesizeMouseAtCenter("#p1", { type: "mousemove" },
                                                 gBrowser.selectedBrowser);
  yield BrowserTestUtils.synthesizeMouseAtCenter("#p1", { }, gBrowser.selectedBrowser);

  // Wait until the tooltip timeout triggers that would normally have opened the popup.
  yield new Promise(resolve => setTimeout(resolve, 0));
  is(document.getElementById("aHTMLTooltip").state, "closed", "local tooltip is closed");
  is(document.getElementById("remoteBrowserTooltip").state, "closed", "remote tooltip is closed");

  gBrowser.removeCurrentTab();
});
