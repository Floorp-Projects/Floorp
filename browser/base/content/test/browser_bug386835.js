var gTestPage = "http://example.org/browser/browser/base/content/test/bug386835.html";
var gTab1, gTab2, gTab3;
var gLevel;

function test() {
  waitForExplicitFinish();

  gTab1 = gBrowser.addTab(gTestPage);
  gTab2 = gBrowser.addTab();
  gTab3 = gBrowser.addTab();
  gBrowser.selectedTab = gTab1;

  gBrowser.getBrowserForTab(gTab1).addEventListener("load", tab1Loaded, true);
}

function tab1Loaded() {
  gBrowser.getBrowserForTab(gTab1).removeEventListener("load", tab1Loaded, true);

  gBrowser.getBrowserForTab(gTab2).addEventListener("load", tab2Loaded, true);
  gBrowser.getBrowserForTab(gTab2).loadURI(gTestPage);
}

function tab2Loaded() {
  gBrowser.getBrowserForTab(gTab2).removeEventListener("load", tab2Loaded, true);

  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1)), 1, "Initial zoom of tab 1 should be 1");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2)), 1, "Initial zoom of tab 2 should be 1");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab3)), 1, "Initial zoom of tab 3 should be 1");

  // Now have three tabs, two with the test page, one blank. Tab 1 is selected
  // Zoom tab 1
  FullZoom.enlarge();
  gLevel = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1));

  ok(gLevel != 1, "New zoom for tab 1 should not be 1");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2)), 1, "Zooming tab 1 should not affect tab 2");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab3)), 1, "Zooming tab 1 should not affect tab 3");

  gBrowser.getBrowserForTab(gTab3).addEventListener("load", tab3Loaded, true);
  gBrowser.getBrowserForTab(gTab3).loadURI(gTestPage);
}

function tab3Loaded() {
  gBrowser.getBrowserForTab(gTab3).removeEventListener("load", tab3Loaded, true);

  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1)), gLevel, "Tab 1 should still be zoomed");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2)), 1, "Tab 2 should still not be affected");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab3)), gLevel, "Tab 3 should have zoomed as it was loading in the background");

  // Switching to tab 2 should update its zoom setting.
  gBrowser.selectedTab = gTab2;

  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1)), gLevel, "Tab 1 should still be zoomed");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2)), gLevel, "Tab 2 should be zoomed now");
  is(ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab3)), gLevel, "Tab 3 should still be zoomed");

  finishTest();
}

function finishTest() {
  FullZoom.reset();
  gBrowser.removeTab(gTab1);
  gBrowser.removeTab(gTab2);
  gBrowser.removeTab(gTab3);
  finish();
}
