function test() {
  waitForExplicitFinish();
  let tab = gBrowser.selectedTab = gBrowser.addTab(
    "http://mochi.test:8888/browser/browser/base/content/test/browser_bug479408_sample.html");
  
  gBrowser.addEventListener("DOMLinkAdded", function(aEvent) {
    gBrowser.removeEventListener("DOMLinkAdded", arguments.callee, true);
    
    executeSoon(function() {
      ok(!tab.linkedBrowser.engines,
         "the subframe's search engine wasn't detected");
      
      gBrowser.removeTab(tab);
      finish();
    });
  }, true);
}
