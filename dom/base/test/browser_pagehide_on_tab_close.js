function test() {
  waitForExplicitFinish();

  var tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;

  tab.linkedBrowser.addEventListener("load", function() {
    tab.linkedBrowser.addEventListener("pagehide", function() {
      ok(true, "got page hide event");
      finish();
    });

    executeSoon(() => { gBrowser.removeTab(tab); });
  }, {capture: true, once: true});
}
