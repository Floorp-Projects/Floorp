function test() {
  gBrowser.pinTab(gBrowser.selectedTab);
  var newTab = gBrowser.duplicateTab(gBrowser.selectedTab);
  ok(!newTab.pinned, "duplicating a pinned tab creates unpinned tab");
  gBrowser.removeTab(newTab);
  gBrowser.unpinTab(gBrowser.selectedTab);
}
