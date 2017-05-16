function test() {
  waitForExplicitFinish();
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser,
    "http://mochi.test:8888/browser/browser/base/content/test/general/browser_bug479408_sample.html");

  gBrowser.addEventListener("DOMLinkAdded", function(aEvent) {
    executeSoon(function() {
      ok(!tab.linkedBrowser.engines,
         "the subframe's search engine wasn't detected");

      gBrowser.removeTab(tab);
      finish();
    });
  }, {capture: true, once: true});
}
