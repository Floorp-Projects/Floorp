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

  function doTest(aIsZoomedWindow, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      if (aIsZoomedWindow) {
        // change the zoom on the blank page
        aWindow.FullZoom.enlarge();
        isnot(aWindow.ZoomManager.zoom, 1, "Zoom level for about:blank should be changed");
      } else {
        // make sure the zoom level is set to 1
        is(aWindow.ZoomManager.zoom, 1, "Zoom level for about:privatebrowsing should be reset");
      }

      aCallback();
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI("about:blank");
  }

  function finishTest() {
    // cleanup
    windowsToReset.forEach(function(win) {
      win.FullZoom.reset();
    });
    finish();
  }

  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      windowsToReset.push(win);
      executeSoon(function() callback(win));
    }, false);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  testOnWindow({}, function(win) {
    doTest(true, win, function() {
      testOnWindow({private: true}, function(win) {
        doTest(false, win, finishTest);
      });
    });
  });
}
