var gTestPage = "http://example.org/browser/browser/base/content/test/dummy_page.html";
var gTestImage = "http://example.org/browser/browser/base/content/test/moz.png";
var gTab1, gTab2, gTab3;
var gLevel;

function test() {
  waitForExplicitFinish();

  gTab1 = gBrowser.addTab(gTestPage);
  gTab2 = gBrowser.addTab();
  gTab3 = gBrowser.addTab();
  gBrowser.selectedTab = gTab1;

  load(gTab1, gTestPage, function () {
    load(gTab2, gTestPage, secondPageLoaded);
  });
}

function secondPageLoaded() {
  zoomTest(gTab1, 1, "Initial zoom of tab 1 should be 1");
  zoomTest(gTab2, 1, "Initial zoom of tab 2 should be 1");
  zoomTest(gTab3, 1, "Initial zoom of tab 3 should be 1");

  // Now have three tabs, two with the test page, one blank. Tab 1 is selected
  // Zoom tab 1
  FullZoom.enlarge();
  gLevel = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1));

  ok(gLevel > 1, "New zoom for tab 1 should be greater than 1");
  zoomTest(gTab2, 1, "Zooming tab 1 should not affect tab 2");
  zoomTest(gTab3, 1, "Zooming tab 1 should not affect tab 3");

  load(gTab3, gTestPage, thirdPageLoaded);
}

function thirdPageLoaded() {
  zoomTest(gTab1, gLevel, "Tab 1 should still be zoomed");
  zoomTest(gTab2, 1, "Tab 2 should still not be affected");
  zoomTest(gTab3, gLevel, "Tab 3 should have zoomed as it was loading in the background");

  // Switching to tab 2 should update its zoom setting.
  gBrowser.selectedTab = gTab2;

  zoomTest(gTab1, gLevel, "Tab 1 should still be zoomed");
  zoomTest(gTab2, gLevel, "Tab 2 should be zoomed now");
  zoomTest(gTab3, gLevel, "Tab 3 should still be zoomed");

  load(gTab1, gTestImage, imageLoaded);
}

function imageLoaded() {
  zoomTest(gTab1, 1, "Zoom should be 1 when image was loaded in the background");
  gBrowser.selectedTab = gTab1;
  zoomTest(gTab1, 1, "Zoom should still be 1 when tab with image is selected");

  finishTest();
}

function finishTest() {
  FullZoom.reset();
  gBrowser.removeTab(gTab1);
  FullZoom.reset();
  gBrowser.removeTab(gTab2);
  FullZoom.reset();
  gBrowser.removeTab(gTab3);
  finish();
}

function zoomTest(tab, val, msg) {
  is(ZoomManager.getZoomForBrowser(tab.linkedBrowser), val, msg);
}

function load(tab, url, cb) {
  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    cb();
  }, true);
  tab.linkedBrowser.loadURI(url);
}
