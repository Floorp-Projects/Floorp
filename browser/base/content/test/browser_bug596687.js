function test() {
  var tab = gBrowser.addTab(null, {skipAnimation: true});
  gBrowser.selectedTab = tab;

  var gotTabAttrModified = false;
  var gotTabClose = false;

  tab.addEventListener("TabClose", function () {
    gotTabClose = true;

    tab.addEventListener("TabAttrModified", function () {
      gotTabAttrModified = true;
    }, false);
  }, false);

  gBrowser.removeTab(tab);

  ok(gotTabClose, "should have got the TabClose event");
  ok(!gotTabAttrModified, "shouldn't have got the TabAttrModified event after TabClose");
}
