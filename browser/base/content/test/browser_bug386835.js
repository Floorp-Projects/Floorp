var gTestPage = "http://example.org/browser/browser/base/content/test/dummy_page.html";
var gTestImage = "http://example.org/browser/browser/base/content/test/moz.png";
var gTab1, gTab2, gTab3;
var gLevel;
const kBack = 0;
const kForward = 1;

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
  zoomTest(gTab1, gLevel, "Zoom should be " + gLevel +" when image was loaded in the background");
  gBrowser.selectedTab = gTab1;
  zoomTest(gTab1, 1, "Zoom should be 1 when tab with image is selected");

  executeSoon(imageZoomSwitch);
}

function imageZoomSwitch() {
  navigate(kBack, function() {
    navigate(kForward, function() {
      zoomTest(gTab1, 1, "Tab 1 should not be zoomed when an image loads");
      gBrowser.selectedTab = gTab2;
      zoomTest(gTab1, 1, "Tab 1 should still not be zoomed when deselected");

      // Mac OS X does not support print preview, so skip those tests
      let isOSX = ("nsILocalFileMac" in Components.interfaces);
      if (isOSX)
        finishTest();
      else
        runPrintPreviewTests();
    });
  });
}

function runPrintPreviewTests() {
  // test print preview on image document
  testPrintPreview(gTab1, function() {
    // test print preview on HTML document
    testPrintPreview(gTab2, function() {
      // test print preview on image document with siteSpecific set to false
      gPrefService.setBoolPref("browser.zoom.siteSpecific", false);
      testPrintPreview(gTab1, function() {
        // test print preview of HTML document with siteSpecific set to false
        testPrintPreview(gTab2, function() {
          gPrefService.clearUserPref("browser.zoom.siteSpecific");
          finishTest();
        });
      });
    });
  });
}

function testPrintPreview(aTab, aCallback) {
  gBrowser.selectedTab = aTab;
  FullZoom.enlarge();
  let level = ZoomManager.zoom;

  function onEnterPP() {
    toggleAffectedChromeOrig.apply(null, arguments);

    function onExitPP() {
      toggleAffectedChromeOrig.apply(null, arguments);
      toggleAffectedChrome = toggleAffectedChromeOrig;

      zoomTest(aTab, level, "Toggling print preview mode should not affect zoom level");

      FullZoom.reset();
      aCallback();
    }
    toggleAffectedChrome = onExitPP;
    PrintUtils.exitPrintPreview();
  }
  let toggleAffectedChromeOrig = toggleAffectedChrome;
  toggleAffectedChrome = onEnterPP;

  let printPreview = new Function(document.getElementById("cmd_printPreview")
                                          .getAttribute("oncommand"));
  executeSoon(printPreview);
}

function finishTest() {
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
  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    cb();
  }, true);
  tab.linkedBrowser.loadURI(url);
}

function navigate(direction, cb) {
  gBrowser.addEventListener("pageshow", function(event) {
    gBrowser.removeEventListener("pageshow", arguments.callee, true);
    setTimeout(cb, 0);
  }, true);
  if (direction == kBack)
    gBrowser.goBack();
  else if (direction == kForward)
    gBrowser.goForward();
}
