function test() {
  waitForExplicitFinish();
  var tab = gBrowser.addTab();
  executeSoon(function () {
    info(window.getComputedStyle(tab).maxWidth);
    gBrowser.removeTab(tab, {animate:true});
    ok(!tab.parentNode, "tab successfully removed");
    finish();
  });
}
