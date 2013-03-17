var tabElm, zoomLevel;
function start_test_prefNotSet() {
  is(ZoomManager.zoom, 1, "initial zoom level should be 1");
  FullZoom.enlarge();

  //capture the zoom level to test later
  zoomLevel = ZoomManager.zoom;
  isnot(zoomLevel, 1, "zoom level should have changed");

  afterZoomAndLoad(continue_test_prefNotSet);
  content.location = 
    "http://mochi.test:8888/browser/browser/base/content/test/moz.png";
}

function continue_test_prefNotSet () {
  is(ZoomManager.zoom, 1, "zoom level pref should not apply to an image");
  FullZoom.reset();

  afterZoomAndLoad(end_test_prefNotSet);
  content.location = 
    "http://mochi.test:8888/browser/browser/base/content/test/zoom_test.html";
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

  tabElm = gBrowser.addTab();
  gBrowser.selectedTab = tabElm;

  afterZoomAndLoad(start_test_prefNotSet);
  content.location = 
    "http://mochi.test:8888/browser/browser/base/content/test/zoom_test.html";
}

function afterZoomAndLoad(cb) {
  let didLoad = false;
  let didZoom = false;
  tabElm.linkedBrowser.addEventListener("load", function() {
    tabElm.linkedBrowser.removeEventListener("load", arguments.callee, true);
    didLoad = true;
    if (didZoom)
      executeSoon(cb);
  }, true);
  let oldSZFB = ZoomManager.setZoomForBrowser;
  ZoomManager.setZoomForBrowser = function(browser, value) {
    oldSZFB.call(ZoomManager, browser, value);
    ZoomManager.setZoomForBrowser = oldSZFB;
    didZoom = true;
    if (didLoad)
      executeSoon(cb);
  };
}
