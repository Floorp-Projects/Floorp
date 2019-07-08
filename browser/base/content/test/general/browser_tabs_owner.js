function test() {
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);

  var owner;

  is(gBrowser.tabs.length, 4, "4 tabs are open");

  owner = gBrowser.selectedTab = gBrowser.tabs[2];
  BrowserOpenTab();
  is(gBrowser.selectedTab, gBrowser.tabs[4], "newly opened tab is selected");
  gBrowser.removeCurrentTab();
  is(gBrowser.selectedTab, owner, "owner is selected");

  owner = gBrowser.selectedTab;
  BrowserOpenTab();
  gBrowser.selectedTab = gBrowser.tabs[1];
  gBrowser.selectedTab = gBrowser.tabs[4];
  gBrowser.removeCurrentTab();
  isnot(
    gBrowser.selectedTab,
    owner,
    "selecting a different tab clears the owner relation"
  );

  owner = gBrowser.selectedTab;
  BrowserOpenTab();
  gBrowser.moveTabTo(gBrowser.selectedTab, 0);
  gBrowser.removeCurrentTab();
  is(
    gBrowser.selectedTab,
    owner,
    "owner relationship persists when tab is moved"
  );

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}
