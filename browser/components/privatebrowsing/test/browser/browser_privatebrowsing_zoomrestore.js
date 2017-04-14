/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that about:privatebrowsing does not appear zoomed in
// if there is already a zoom site pref for about:blank (bug 487656).

add_task(function* test() {
  // initialization
  let windowsToClose = [];
  let windowsToReset = [];

  function promiseLocationChange() {
    return new Promise(resolve => {
      Services.obs.addObserver(function onLocationChange(subj, topic, data) {
        Services.obs.removeObserver(onLocationChange, topic);
        resolve();
      }, "browser-fullZoom:location-change");
    });
  }

  function promiseTestReady(aIsZoomedWindow, aWindow) {
    // Need to wait on two things, the ordering of which is not guaranteed:
    // (1) the page load, and (2) FullZoom's update to the new page's zoom
    // level.  FullZoom broadcasts "browser-fullZoom:location-change" when its
    // update is done.  (See bug 856366 for details.)


    let browser = aWindow.gBrowser.selectedBrowser;
    return BrowserTestUtils.loadURI(browser, "about:blank").then(() => {
      return Promise.all([ BrowserTestUtils.browserLoaded(browser),
                           promiseLocationChange() ]);
    }).then(() => doTest(aIsZoomedWindow, aWindow));
  }

  function doTest(aIsZoomedWindow, aWindow) {
    if (aIsZoomedWindow) {
      is(aWindow.ZoomManager.zoom, 1,
         "Zoom level for freshly loaded about:blank should be 1");
      // change the zoom on the blank page
      aWindow.FullZoom.enlarge();
      isnot(aWindow.ZoomManager.zoom, 1, "Zoom level for about:blank should be changed");
      return;
    }

    // make sure the zoom level is set to 1
    is(aWindow.ZoomManager.zoom, 1, "Zoom level for about:privatebrowsing should be reset");
  }

  function testOnWindow(options, callback) {
    return BrowserTestUtils.openNewBrowserWindow(options).then((win) => {
      windowsToClose.push(win);
      windowsToReset.push(win);
      return win;
    });
  }

  yield testOnWindow({}).then(win => promiseTestReady(true, win));
  yield testOnWindow({private: true}).then(win => promiseTestReady(false, win));

  // cleanup
  windowsToReset.forEach((win) => win.FullZoom.reset());
  yield Promise.all(windowsToClose.map(win => BrowserTestUtils.closeWindow(win)));
});
