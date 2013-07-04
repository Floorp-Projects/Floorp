var tabElm, zoomLevel;
function start_test_prefNotSet() {
  Task.spawn(function () {
    is(ZoomManager.zoom, 1, "initial zoom level should be 1");
    FullZoom.enlarge();

    //capture the zoom level to test later
    zoomLevel = ZoomManager.zoom;
    isnot(zoomLevel, 1, "zoom level should have changed");

    yield FullZoomHelper.load(gBrowser.selectedTab, "http://mochi.test:8888/browser/browser/base/content/test/moz.png");
  }).then(continue_test_prefNotSet, FullZoomHelper.failAndContinue(finish));
}

function continue_test_prefNotSet () {
  Task.spawn(function () {
    is(ZoomManager.zoom, 1, "zoom level pref should not apply to an image");
    FullZoom.reset();

    yield FullZoomHelper.load(gBrowser.selectedTab, "http://mochi.test:8888/browser/browser/base/content/test/zoom_test.html");
  }).then(end_test_prefNotSet, FullZoomHelper.failAndContinue(finish));
}

function end_test_prefNotSet() {
  is(ZoomManager.zoom, zoomLevel, "the zoom level should have persisted");

  // Reset the zoom so that other tests have a fresh zoom level
  FullZoom.reset();
  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function () {
    tabElm = gBrowser.addTab();
    yield FullZoomHelper.selectTabAndWaitForLocationChange(tabElm);
    yield FullZoomHelper.load(tabElm, "http://mochi.test:8888/browser/browser/base/content/test/zoom_test.html");
  }).then(start_test_prefNotSet, FullZoomHelper.failAndContinue(finish));
}
