function test() {
  gBrowser.selectedBrowser.focus();
  BrowserOpenTab();
  ok(gURLBar.focused, "location bar is focused for a new tab");

  gBrowser.selectedTab = gBrowser.tabs[0];
  ok(!gURLBar.focused, "location bar isn't focused for the previously selected tab");

  gBrowser.selectedTab = gBrowser.tabs[1];
  ok(gURLBar.focused, "location bar is re-focused when selecting the new tab");

  gBrowser.removeCurrentTab();
}
