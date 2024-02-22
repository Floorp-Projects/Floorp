add_task(async function () {
  const html =
    '<p id="p1" title="tooltip is here">This paragraph has a tooltip.</p>';
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html," + html
  );

  await SpecialPowers.pushPrefEnv({ set: [["ui.tooltip.delay_ms", 0]] });

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#p1",
    { type: "mousemove" },
    gBrowser.selectedBrowser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#p1",
    {},
    gBrowser.selectedBrowser
  );

  // Wait until the tooltip timeout triggers that would normally have opened the popup.
  await new Promise(resolve => setTimeout(resolve, 0));
  is(
    document.getElementById("aHTMLTooltip").state,
    "closed",
    "local tooltip is closed"
  );
  is(
    document.getElementById("remoteBrowserTooltip").state,
    "closed",
    "remote tooltip is closed"
  );

  gBrowser.removeCurrentTab();
});
