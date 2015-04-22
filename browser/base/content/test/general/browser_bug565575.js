add_task(function* () {
  gBrowser.selectedBrowser.focus();

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => BrowserOpenTab(), false);
  ok(gURLBar.focused, "location bar is focused for a new tab");

  yield BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  ok(!gURLBar.focused, "location bar isn't focused for the previously selected tab");

  yield BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[1]);
  ok(gURLBar.focused, "location bar is re-focused when selecting the new tab");

  gBrowser.removeCurrentTab();
});
