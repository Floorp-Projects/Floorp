function test() {
  waitForExplicitFinish();

  let testPage = "http://example.org/browser/browser/base/content/test/dummy_page.html";
  let tab1 = gBrowser.selectedTab = gBrowser.addTab();
  tab1.linkedBrowser.addEventListener("load", (function(event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);

    let tab2 = gBrowser.addTab();
    tab2.linkedBrowser.addEventListener("load", (function(event) {
      event.currentTarget.removeEventListener("load", arguments.callee, true);

      FullZoom.enlarge();
      let tab1Zoom = ZoomManager.getZoomForBrowser(tab1.linkedBrowser);
      gBrowser.selectedTab = tab2;
      let tab2Zoom = ZoomManager.getZoomForBrowser(tab2.linkedBrowser);
      is(tab2Zoom, tab1Zoom, "Zoom should affect background tabs");

      gPrefService.setBoolPref("browser.zoom.updateBackgroundTabs", false);
      FullZoom.reset();
      gBrowser.selectedTab = tab1;
      tab1Zoom = ZoomManager.getZoomForBrowser(tab1.linkedBrowser);
      tab2Zoom = ZoomManager.getZoomForBrowser(tab2.linkedBrowser);
      isnot(tab1Zoom, tab2Zoom, "Zoom should not affect background tabs");

      if (gPrefService.prefHasUserValue("browser.zoom.updateBackgroundTabs"))
        gPrefService.clearUserPref("browser.zoom.updateBackgroundTabs");
      gBrowser.removeTab(tab1);
      gBrowser.removeTab(tab2);
      finish();
    }), true);
    tab2.linkedBrowser.loadURI(testPage);
  }), true);
  content.location = testPage;
}
