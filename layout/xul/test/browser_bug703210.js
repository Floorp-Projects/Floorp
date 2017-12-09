add_task(async function() {
  const url = "data:text/html," +
    "<html onmousemove='event.stopPropagation()'" +
    " onmouseenter='event.stopPropagation()' onmouseleave='event.stopPropagation()'" +
    " onmouseover='event.stopPropagation()' onmouseout='event.stopPropagation()'>" +
    "<p id=\"p1\" title=\"tooltip is here\">This paragraph has a tooltip.</p>" +
    "<p id=\"p2\">This paragraph doesn't have tooltip.</p></html>";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;

  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [["ui.tooltipDelay", 0]]}, resolve);
  });

  // Send a mousemove at a known position to start the test.
  await BrowserTestUtils.synthesizeMouseAtCenter("#p2", { type: "mousemove" }, browser);
  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown", false, event => {
    is(event.originalTarget.localName, "tooltip", "tooltip is showing");
    return true;
  });
  await BrowserTestUtils.synthesizeMouseAtCenter("#p1", { type: "mousemove" }, browser);
  await popupShownPromise;

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden", false, event => {
    is(event.originalTarget.localName, "tooltip", "tooltip is hidden");
    return true;
  });
  await BrowserTestUtils.synthesizeMouseAtCenter("#p2", { type: "mousemove" }, browser);
  await popupHiddenPromise;

  gBrowser.removeCurrentTab();
});
