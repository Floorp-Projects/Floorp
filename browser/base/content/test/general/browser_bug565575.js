add_task(async function () {
  gBrowser.selectedBrowser.focus();

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => BrowserOpenTab(),
    false
  );
  ok(gURLBar.focused, "location bar is focused for a new tab");

  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  ok(
    !gURLBar.focused,
    "location bar isn't focused for the previously selected tab"
  );

  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[1]);
  ok(gURLBar.focused, "location bar is re-focused when selecting the new tab");

  gBrowser.removeCurrentTab();
});
