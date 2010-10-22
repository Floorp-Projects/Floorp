function test() {
  waitForExplicitFinish();
  var tab = gBrowser.addTab();
  executeSoon(function () {
    gBrowser.removeTab(tab, {animate:true});
    ok(!tab.parentNode, "tab successfully removed");
    finish();
  });
}
