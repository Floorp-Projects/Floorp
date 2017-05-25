function test() {
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.addTab(gBrowser);

  var tabs = gBrowser.tabs;
  var owner;

  is(tabs.length, 4, "4 tabs are open");

  owner = gBrowser.selectedTab = tabs[2];
  BrowserOpenTab();
  is(gBrowser.selectedTab, tabs[4], "newly opened tab is selected");
  gBrowser.removeCurrentTab();
  is(gBrowser.selectedTab, owner, "owner is selected");

  owner = gBrowser.selectedTab;
  BrowserOpenTab();
  gBrowser.selectedTab = tabs[1];
  gBrowser.selectedTab = tabs[4];
  gBrowser.removeCurrentTab();
  isnot(gBrowser.selectedTab, owner, "selecting a different tab clears the owner relation");

  owner = gBrowser.selectedTab;
  BrowserOpenTab();
  gBrowser.moveTabTo(gBrowser.selectedTab, 0);
  gBrowser.removeCurrentTab();
  is(gBrowser.selectedTab, owner, "owner relatitionship persists when tab is moved");

  while (tabs.length > 1)
    gBrowser.removeCurrentTab();
}
