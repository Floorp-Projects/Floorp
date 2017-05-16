function test() {
  waitForExplicitFinish();

  var tab = BrowserTestUtils.addTab(gBrowser);

  tab.addEventListener("TabClose", function() {
    ok(tab.linkedBrowser, "linkedBrowser should still exist during the TabClose event");

    executeSoon(function() {
      ok(!tab.linkedBrowser, "linkedBrowser should be gone after the TabClose event");

      finish();
    });
  }, {once: true});

  gBrowser.removeTab(tab);
}
