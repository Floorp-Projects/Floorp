var gTestPage = "http://example.org/browser/browser/base/content/test/dummy_page.html";
var gTestImage = "http://example.org/browser/browser/base/content/test/moz.png";
var gTab1, gTab2, gTab3;
var gLevel;
const BACK = 0;
const FORWARD = 1;

function test() {
  waitForExplicitFinish();

  Task.spawn(function () {
    gTab1 = gBrowser.addTab(gTestPage);
    gTab2 = gBrowser.addTab();
    gTab3 = gBrowser.addTab();

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    yield FullZoomHelper.load(gTab1, gTestPage);
    yield FullZoomHelper.load(gTab2, gTestPage);
  }).then(secondPageLoaded, FullZoomHelper.failAndContinue(finish));
}

function secondPageLoaded() {
  Task.spawn(function () {
    FullZoomHelper.zoomTest(gTab1, 1, "Initial zoom of tab 1 should be 1");
    FullZoomHelper.zoomTest(gTab2, 1, "Initial zoom of tab 2 should be 1");
    FullZoomHelper.zoomTest(gTab3, 1, "Initial zoom of tab 3 should be 1");

    // Now have three tabs, two with the test page, one blank. Tab 1 is selected
    // Zoom tab 1
    FullZoom.enlarge();
    gLevel = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1));

    ok(gLevel > 1, "New zoom for tab 1 should be greater than 1");
    FullZoomHelper.zoomTest(gTab2, 1, "Zooming tab 1 should not affect tab 2");
    FullZoomHelper.zoomTest(gTab3, 1, "Zooming tab 1 should not affect tab 3");

    yield FullZoomHelper.load(gTab3, gTestPage);
  }).then(thirdPageLoaded, FullZoomHelper.failAndContinue(finish));
}

function thirdPageLoaded() {
  Task.spawn(function () {
    FullZoomHelper.zoomTest(gTab1, gLevel, "Tab 1 should still be zoomed");
    FullZoomHelper.zoomTest(gTab2, 1, "Tab 2 should still not be affected");
    FullZoomHelper.zoomTest(gTab3, gLevel, "Tab 3 should have zoomed as it was loading in the background");

    // Switching to tab 2 should update its zoom setting.
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    FullZoomHelper.zoomTest(gTab1, gLevel, "Tab 1 should still be zoomed");
    FullZoomHelper.zoomTest(gTab2, gLevel, "Tab 2 should be zoomed now");
    FullZoomHelper.zoomTest(gTab3, gLevel, "Tab 3 should still be zoomed");

    yield FullZoomHelper.load(gTab1, gTestImage);
  }).then(imageLoaded, FullZoomHelper.failAndContinue(finish));
}

function imageLoaded() {
  Task.spawn(function () {
    FullZoomHelper.zoomTest(gTab1, 1, "Zoom should be 1 when image was loaded in the background");
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    FullZoomHelper.zoomTest(gTab1, 1, "Zoom should still be 1 when tab with image is selected");
  }).then(imageZoomSwitch, FullZoomHelper.failAndContinue(finish));
}

function imageZoomSwitch() {
  Task.spawn(function () {
    yield FullZoomHelper.navigate(BACK);
    yield FullZoomHelper.navigate(FORWARD);
    FullZoomHelper.zoomTest(gTab1, 1, "Tab 1 should not be zoomed when an image loads");

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    FullZoomHelper.zoomTest(gTab1, 1, "Tab 1 should still not be zoomed when deselected");
  }).then(finishTest, FullZoomHelper.failAndContinue(finish));
}

var finishTestStarted  = false;
function finishTest() {
  Task.spawn(function () {
    ok(!finishTestStarted, "finishTest called more than once");
    finishTestStarted = true;
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    FullZoom.reset();
    gBrowser.removeTab(gTab1);
    FullZoom.reset();
    gBrowser.removeTab(gTab2);
    FullZoom.reset();
    gBrowser.removeTab(gTab3);
  }).then(finish, FullZoomHelper.failAndContinue(finish));
}
