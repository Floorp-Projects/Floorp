var gTestPage = "http://example.org/browser/browser/base/content/test/dummy_page.html";
var gTestImage = "http://example.org/browser/browser/base/content/test/moz.png";
var gTab1, gTab2, gTab3;
var gLevel;
const BACK = 0;
const FORWARD = 1;

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
  afterZoom(function() {
    zoomTest(gTab1, gLevel, "Tab 1 should still be zoomed");
    zoomTest(gTab2, gLevel, "Tab 2 should be zoomed now");
    zoomTest(gTab3, gLevel, "Tab 3 should still be zoomed");

    load(gTab1, gTestImage, imageLoaded);
  });

  gBrowser.selectedTab = gTab2;
}

function imageLoaded() {
  zoomTest(gTab1, 1, "Zoom should be 1 when image was loaded in the background");
  afterZoom(function() {
    zoomTest(gTab1, 1, "Zoom should still be 1 when tab with image is selected");

    executeSoon(imageZoomSwitch);
  });
  gBrowser.selectedTab = gTab1;
}

function imageZoomSwitch() {
  navigate(BACK, function () {
    navigate(FORWARD, function () {
      zoomTest(gTab1, 1, "Tab 1 should not be zoomed when an image loads");

      afterZoom(function() {
        zoomTest(gTab1, 1, "Tab 1 should still not be zoomed when deselected");
        finishTest();
      });
      gBrowser.selectedTab = gTab2;
    });
  });
}

var finishTestStarted  = false;
function finishTest() {
  ok(!finishTestStarted, "finishTest called more than once");
  finishTestStarted = true;
  gBrowser.selectedTab = gTab1;
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
  let didLoad = false;
  let didZoom = false;
  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    didLoad = true;
    if (didZoom)
      executeSoon(cb);
  }, true);

  afterZoom(function() {
    didZoom = true;
    if (didLoad)
      executeSoon(cb);
  });

  tab.linkedBrowser.loadURI(url);
}

function navigate(direction, cb) {
  let didPs = false;
  let didZoom = false;
  gBrowser.addEventListener("pageshow", function (event) {
    gBrowser.removeEventListener("pageshow", arguments.callee, true);
    didPs = true;
    if (didZoom)
      executeSoon(cb);
  }, true);

  afterZoom(function() {
    didZoom = true;
    if (didPs)
      executeSoon(cb);
  });

  if (direction == BACK)
    gBrowser.goBack();
  else if (direction == FORWARD)
    gBrowser.goForward();
}

function afterZoom(cb) {
  let oldAPTS = FullZoom._applyPrefToSetting;
  FullZoom._applyPrefToSetting = function(value, browser) {
    if (!value)
      value = undefined;
    oldAPTS.call(FullZoom, value, browser);
    FullZoom._applyPrefToSetting = oldAPTS;
    executeSoon(cb);
  };
}
