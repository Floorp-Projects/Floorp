/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that about:privatebrowsing does not appear zoomed in
// if there is already a zoom site pref for about:blank (bug 487656).

function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let windowsToReset = [];

  function doTestWhenReady(aIsZoomedWindow, aWindow, aCallback) {
    // Need to wait on two things, the ordering of which is not guaranteed:
    // (1) the page load, and (2) FullZoom's update to the new page's zoom
    // level.  FullZoom broadcasts "browser-fullZoom:locationChange" when its
    // update is done.  (See bug 856366 for details.)

    let n = 0;

    let browser = aWindow.gBrowser.selectedBrowser;
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      if (++n == 2)
        doTest(aIsZoomedWindow, aWindow, aCallback);
    }, true);

    let topic = "browser-fullZoom:locationChange";
    Services.obs.addObserver(function onLocationChange() {
      Services.obs.removeObserver(onLocationChange, topic);
      if (++n == 2)
        doTest(aIsZoomedWindow, aWindow, aCallback);
    }, topic, false);

    browser.loadURI("about:blank");
  }

  function doTest(aIsZoomedWindow, aWindow, aCallback) {
    if (aIsZoomedWindow) {
      is(aWindow.ZoomManager.zoom, 1,
         "Zoom level for freshly loaded about:blank should be 1");
      // change the zoom on the blank page
      aWindow.FullZoom.enlarge();
      isnot(aWindow.ZoomManager.zoom, 1, "Zoom level for about:blank should be changed");
      aCallback();
      return;
    }
    // make sure the zoom level is set to 1
    is(aWindow.ZoomManager.zoom, 1, "Zoom level for about:privatebrowsing should be reset");
    aCallback();
  }

  function finishTest() {
    // cleanup
    windowsToReset.forEach(function(win) {
      win.FullZoom.reset();
    });
    windowsToClose.forEach(function(win) {
      win.close();
    });
    finish();
  }

  function testOnWindow(options, callback) {
    let win = whenNewWindowLoaded(options,
      function(win) {
        windowsToClose.push(win);
        windowsToReset.push(win);
        executeSoon(function() { callback(win); });
      });
  };

  testOnWindow({}, function(win) {
    doTestWhenReady(true, win, function() {
      testOnWindow({private: true}, function(win) {
        doTestWhenReady(false, win, finishTest);
      });
    });
  });
}
