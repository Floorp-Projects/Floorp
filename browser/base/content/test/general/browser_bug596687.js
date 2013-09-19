function test() {
  var tab = gBrowser.addTab(null, {skipAnimation: true});
  gBrowser.selectedTab = tab;

  var gotTabAttrModified = false;
  var gotTabClose = false;

  function onTabClose() {
    gotTabClose = true;
    tab.addEventListener("TabAttrModified", onTabAttrModified, false);
  }

  function onTabAttrModified() {
    gotTabAttrModified = true;
  }

  tab.addEventListener("TabClose", onTabClose, false);

  gBrowser.removeTab(tab);

  ok(gotTabClose, "should have got the TabClose event");
  ok(!gotTabAttrModified, "shouldn't have got the TabAttrModified event after TabClose");

  tab.removeEventListener("TabClose", onTabClose, false);
  tab.removeEventListener("TabAttrModified", onTabAttrModified, false);
}
