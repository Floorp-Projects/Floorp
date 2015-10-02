function test() {
  var tab1 = gBrowser.selectedTab;
  var tab2 = gBrowser.addTab();
  var childTab1;
  var childTab2;

  childTab1 = gBrowser.addTab("about:blank", { relatedToCurrent: true });
  gBrowser.selectedTab = childTab1;
  gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
  is(idx(gBrowser.selectedTab), idx(tab1),
     "closing a tab next to its parent selects the parent");

  childTab1 = gBrowser.addTab("about:blank", { relatedToCurrent: true });
  gBrowser.selectedTab = tab2;
  gBrowser.selectedTab = childTab1;
  gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
  is(idx(gBrowser.selectedTab), idx(tab2),
     "closing a tab next to its parent doesn't select the parent if another tab had been selected ad interim");

  gBrowser.selectedTab = tab1;
  childTab1 = gBrowser.addTab("about:blank", { relatedToCurrent: true });
  childTab2 = gBrowser.addTab("about:blank", { relatedToCurrent: true });
  gBrowser.selectedTab = childTab1;
  gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
  is(idx(gBrowser.selectedTab), idx(childTab2),
     "closing a tab next to its parent selects the next tab with the same parent");
  gBrowser.removeTab(gBrowser.selectedTab, { skipPermitUnload: true });
  is(idx(gBrowser.selectedTab), idx(tab2),
     "closing the last tab in a set of child tabs doesn't go back to the parent");

  gBrowser.removeTab(tab2, { skipPermitUnload: true });
}

function idx(tab) {
  return Array.indexOf(gBrowser.tabs, tab);
}
