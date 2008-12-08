var tabElm, zoomLevel;
function start_test_prefNotSet() {
  tabElm.linkedBrowser.removeEventListener("load", start_test_prefNotSet, true);
  tabElm.linkedBrowser.addEventListener("load", continue_test_prefNotSet, true);

  is(ZoomManager.zoom, 1, "initial zoom level should be 1");
  FullZoom.enlarge();

  //capture the zoom level to test later
  zoomLevel = ZoomManager.zoom;
  isnot(zoomLevel, 1, "zoom level should have changed");

  content.location = 
    "http://localhost:8888/browser/browser/base/content/test/moz.png";
}

function continue_test_prefNotSet () {
  tabElm.linkedBrowser.removeEventListener("load", continue_test_prefNotSet, true);
  tabElm.linkedBrowser.addEventListener("load", end_test_prefNotSet, true);

  is(ZoomManager.zoom, 1, "zoom level pref should not apply to an image");
  FullZoom.reset();

  content.location = 
    "http://localhost:8888/browser/browser/base/content/test/zoom_test.html";
}

function end_test_prefNotSet() {
  is(ZoomManager.zoom, zoomLevel, "the zoom level should have persisted");

  gBrowser.removeCurrentTab();
  finish();
}


function test() {
  waitForExplicitFinish();

  tabElm = gBrowser.addTab();
  gBrowser.selectedTab = tabElm;
  tabElm.linkedBrowser.addEventListener("load", start_test_prefNotSet, true);

  content.location = 
    "http://localhost:8888/browser/browser/base/content/test/zoom_test.html";

}
